#include "CelestialEnvironmentProfile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace world::celestial::visual
{

namespace
{

using json = nlohmann::json;

std::string normalizeEnvironmentToken(
    std::string s
)
{
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';

        if (c == ' ' || c == '-')
            c = '_';

        c =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(c)
                )
            );
    }

    std::string out;
    out.reserve(s.size());

    bool lastUnderscore = false;

    for (char c : s)
    {
        const bool keep =
            std::isalnum(
                static_cast<unsigned char>(c)
            ) ||
            c == '_';

        if (!keep)
            continue;

        if (c == '_')
        {
            if (lastUnderscore)
                continue;

            lastUnderscore = true;
        }
        else
        {
            lastUnderscore = false;
        }

        out.push_back(c);
    }

    while (!out.empty() && out.front() == '_')
        out.erase(out.begin());

    while (!out.empty() && out.back() == '_')
        out.pop_back();

    return out;
}

std::filesystem::path resolveProjectPath(
    const std::string& pathText
)
{
    namespace fs = std::filesystem;

    const fs::path raw(pathText);

    if (raw.is_absolute())
        return raw.lexically_normal();

    const fs::path cwd =
        fs::current_path();

    const std::vector<fs::path> candidates =
    {
        cwd / raw,
        cwd.parent_path() / raw,
        cwd.parent_path().parent_path() / raw,
        fs::path("D:/__elite/work") / raw
    };

    for (const fs::path& p : candidates)
    {
        if (fs::exists(p))
            return p.lexically_normal();
    }

    return (cwd.parent_path() / raw).lexically_normal();
}

bool loadJsonFile(
    const std::filesystem::path& path,
    json& out
)
{
    std::ifstream f(path);

    if (!f.is_open())
        return false;

    try
    {
        f >> out;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[CelestialEnvironmentProfileLibrary] JSON parse error: "
            << path.generic_string()
            << " : "
            << e.what()
            << "\n";

        return false;
    }

    return true;
}

std::string jsonString(
    const json& object,
    const std::string& key,
    const std::string& fallback = ""
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_string())
        return fallback;

    return object[key].get<std::string>();
}

bool jsonBool(
    const json& object,
    const std::string& key,
    bool fallback = false
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_boolean())
        return fallback;

    return object[key].get<bool>();
}

float jsonFloat(
    const json& object,
    const std::string& key,
    float fallback = 0.0f
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_number())
        return fallback;

    return object[key].get<float>();
}








glm::vec2 jsonVec2(
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






glm::ivec2 jsonIVec2(
    const json& object,
    const std::string& key,
    const glm::ivec2& fallback
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
        !value[0].is_number_integer() ||
        !value[1].is_number_integer())
    {
        return fallback;
    }

    return glm::ivec2(
        std::max(
            1,
            value[0].get<int>()
        ),
        std::max(
            1,
            value[1].get<int>()
        )
    );
}







std::uint32_t jsonUint32(
    const json& object,
    const std::string& key,
    std::uint32_t fallback = 0u
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_number_unsigned() &&
        !object[key].is_number_integer())
    {
        return fallback;
    }

    const long long value =
        object[key].get<long long>();

    if (value < 0)
        return fallback;

    return static_cast<std::uint32_t>(value);
}

glm::vec3 jsonVec3(
    const json& object,
    const std::string& key,
    const glm::vec3& fallback
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    const json& a =
        object[key];

    if (!a.is_array() ||
        a.size() < 3)
    {
        return fallback;
    }

    return glm::vec3(
        a[0].get<float>(),
        a[1].get<float>(),
        a[2].get<float>()
    );
}

glm::vec4 jsonVec4(
    const json& object,
    const std::string& key,
    const glm::vec4& fallback
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    const json& a =
        object[key];

    if (!a.is_array() ||
        a.size() < 4)
    {
        return fallback;
    }

    return glm::vec4(
        a[0].get<float>(),
        a[1].get<float>(),
        a[2].get<float>(),
        a[3].get<float>()
    );
}

EnvironmentCloudPattern parseCloudPattern(
    const std::string& text
)
{
    const std::string value =
        normalizeEnvironmentToken(text);

    if (value == "scattered_cumulus")
        return EnvironmentCloudPattern::ScatteredCumulus;

    if (value == "broken_fields")
        return EnvironmentCloudPattern::BrokenFields;

    if (value == "dense_overcast")
        return EnvironmentCloudPattern::DenseOvercast;

    if (value == "banded" ||
        value == "zonal_bands")
    {
        return EnvironmentCloudPattern::Banded;
    }

    return EnvironmentCloudPattern::None;
}









bool readEnvironmentDocument(
    const std::filesystem::path& path,
    CelestialEnvironmentProfile& out
)
{
    json root;

    if (!loadJsonFile(path, root))
        return false;

    out = CelestialEnvironmentProfile{};
    out.found = true;

    out.presetId =
        jsonString(
            root,
            "preset_id",
            ""
        );

    out.classification =
        jsonString(
            root,
            "classification",
            ""
        );

    out.dataQuality =
        jsonString(
            root,
            "data_quality",
            ""
        );

    out.systemFolderName =
        jsonString(
            root,
            "system_folder_name",
            ""
        );

    out.bodyFolderName =
        jsonString(
            root,
            "body_folder_name",
            path.stem().generic_string()
        );

    out.displayName =
        jsonString(
            root,
            "display_name",
            out.bodyFolderName
        );

    const json atmosphere =
        root.value(
            "atmosphere",
            json::object()
        );

    out.atmosphere.enabled =
        jsonBool(
            atmosphere,
            "enabled",
            false
        );

    out.atmosphere.kind =
        jsonString(
            atmosphere,
            "kind",
            ""
        );

    out.atmosphere.visualIntensity =
        jsonFloat(
            atmosphere,
            "visual_intensity",
            1.0f
        );

    out.atmosphere.radiusScale =
        jsonFloat(
            atmosphere,
            "radius_scale",
            1.018f
        );

    const json physical =
        atmosphere.value(
            "physical",
            json::object()
        );

    out.atmosphere.physical.surfacePressurePa =
        jsonFloat(
            physical,
            "surface_pressure_pa",
            0.0f
        );

    out.atmosphere.physical.scaleHeightKm =
        jsonFloat(
            physical,
            "scale_height_km",
            0.0f
        );

    out.atmosphere.physical.outerHeightKm =
        jsonFloat(
            physical,
            "outer_height_km",
            0.0f
        );

    const json haze =
        atmosphere.value(
            "haze",
            json::object()
        );

    out.atmosphere.haze.centerStrength =
        jsonFloat(haze, "center_strength", 0.0f);

    out.atmosphere.haze.limbStrength =
        jsonFloat(haze, "limb_strength", 0.0f);

    out.atmosphere.haze.contrastLoss =
        jsonFloat(haze, "contrast_loss", 0.0f);

    out.atmosphere.haze.desaturation =
        jsonFloat(haze, "desaturation", 0.0f);

    out.atmosphere.haze.distortionStrength =
        jsonFloat(haze, "distortion_strength", 0.0f);

    const json atmosphereColors =
        atmosphere.value(
            "colors",
            json::object()
        );

    out.atmosphere.surfaceHaze =
        jsonVec4(
            atmosphereColors,
            "surface_haze",
            glm::vec4(0.0f)
        );

    out.atmosphere.limbCore =
        jsonVec4(
            atmosphereColors,
            "limb_core",
            glm::vec4(0.0f)
        );

    out.atmosphere.nearAtmosphere =
        jsonVec4(
            atmosphereColors,
            "near_atmosphere",
            glm::vec4(0.0f)
        );

    out.atmosphere.outerAtmosphere =
        jsonVec4(
            atmosphereColors,
            "outer_atmosphere",
            glm::vec4(0.0f)
        );

    const json clouds =
        root.value(
            "clouds",
            json::object()
        );

    out.clouds.enabled =
        jsonBool(
            clouds,
            "enabled",
            false
        );

    out.clouds.pattern =
        parseCloudPattern(
            jsonString(
                clouds,
                "pattern",
                "none"
            )
        );

    out.clouds.globalCoverage =
        std::clamp(
            jsonFloat(
                clouds,
                "global_coverage",
                0.0f
            ),
            0.0f,
            1.0f
        );

    const json appearance =
        clouds.value(
            "appearance",
            json::object()
        );

    out.clouds.appearance.baseColor =
        jsonVec3(
            appearance,
            "base_color",
            out.clouds.appearance.baseColor
        );

    out.clouds.appearance.shadowColor =
        jsonVec3(
            appearance,
            "shadow_color",
            out.clouds.appearance.shadowColor
        );

    out.clouds.appearance.saturation =
        jsonFloat(
            appearance,
            "saturation",
            0.0f
        );

    out.clouds.appearance.contrast =
        jsonFloat(
            appearance,
            "contrast",
            1.0f
        );

    if (clouds.contains("layers") &&
        clouds["layers"].is_array())
    {
        for (const json& sourceLayer : clouds["layers"])
        {
            if (!sourceLayer.is_object())
                continue;

            EnvironmentCloudLayerProfile layer;

            layer.id =
                jsonString(sourceLayer, "id", "");

            layer.type =
                jsonString(sourceLayer, "type", "");

            layer.renderRole =
                jsonString(
                    sourceLayer,
                    "render_role",
                    ""
                );

            layer.baseHeightKm =
                jsonFloat(sourceLayer, "base_height_km", 0.0f);

            layer.topHeightKm =
                jsonFloat(sourceLayer, "top_height_km", 0.0f);

            layer.coverage =
                std::clamp(
                    jsonFloat(sourceLayer, "coverage", 0.0f),
                    0.0f,
                    1.0f
                );

            layer.density =
                std::clamp(
                    jsonFloat(sourceLayer, "density", 0.0f),
                    0.0f,
                    1.0f
                );

            layer.opacity =
                std::clamp(
                    jsonFloat(sourceLayer, "opacity", 0.0f),
                    0.0f,
                    1.0f
                );

            layer.particleScale =
                std::max(
                    0.01f,
                    jsonFloat(
                        sourceLayer,
                        "particle_scale",
                        1.0f
                    )
                );


            const json layerAppearance =
                sourceLayer.value(
                    "appearance",
                    json::object()
                );

            layer.appearance.baseColor =
                jsonVec3(
                    layerAppearance,
                    "base_color",
                    out.clouds.appearance.baseColor
                );

            layer.appearance.shadowColor =
                jsonVec3(
                    layerAppearance,
                    "shadow_color",
                    out.clouds.appearance.shadowColor
                );

            layer.appearance.opacityScale =
                std::max(
                    0.0f,
                    jsonFloat(
                        layerAppearance,
                        "opacity_scale",
                        1.0f
                    )
                );



            const json wind =
                sourceLayer.value(
                    "wind",
                    json::object()
                );

            const glm::vec2 speedRange =
                jsonVec2(
                    wind,
                    "speed_range_mps",
                    glm::vec2(0.0f)
                );

            layer.wind.minimumSpeedMps =
                std::max(0.0f, speedRange.x);

            layer.wind.maximumSpeedMps =
                std::max(
                    layer.wind.minimumSpeedMps,
                    speedRange.y
                );

            layer.wind.predominantDirectionDeg =
                jsonFloat(
                    wind,
                    "predominant_direction_deg",
                    0.0f
                );

            layer.wind.dataStatus =
                jsonString(
                    wind,
                    "data_status",
                    ""
                );

            out.clouds.layers.push_back(
                std::move(layer)
            );
        }
    }

    const json circulation =
        clouds.value(
            "circulation",
            json::object()
        );

    out.clouds.circulation.model =
        jsonString(
            circulation,
            "model",
            "none"
        );

    if (circulation.contains("zones") &&
        circulation["zones"].is_array())
    {
        for (const json& sourceZone : circulation["zones"])
        {
            if (!sourceZone.is_object())
                continue;

            EnvironmentCloudCirculationZone zone;

            zone.id =
                jsonString(sourceZone, "id", "");

            zone.latitudeCenterDeg =
                jsonFloat(
                    sourceZone,
                    "latitude_center_deg",
                    0.0f
                );

            zone.halfWidthDeg =
                std::max(
                    0.01f,
                    jsonFloat(
                        sourceZone,
                        "half_width_deg",
                        1.0f
                    )
                );

            zone.coverageMultiplier =
                jsonFloat(
                    sourceZone,
                    "coverage_multiplier",
                    1.0f
                );

            zone.densityMultiplier =
                jsonFloat(
                    sourceZone,
                    "density_multiplier",
                    1.0f
                );

            zone.fragmentation =
                std::clamp(
                    jsonFloat(
                        sourceZone,
                        "fragmentation",
                        0.0f
                    ),
                    0.0f,
                    1.0f
                );

            zone.zonalStretch =
                std::max(
                    0.05f,
                    jsonFloat(
                        sourceZone,
                        "zonal_stretch",
                        1.0f
                    )
                );

            zone.driftSpeedMultiplier =
                jsonFloat(
                    sourceZone,
                    "drift_speed_multiplier",
                    1.0f
                );

            out.clouds.circulation.zones.push_back(
                std::move(zone)
            );
        }
    }

    const json generation =
        clouds.value(
            "generation",
            json::object()
        );

    auto& target =
        out.clouds.generation;

    target.seedPolicy =
        jsonString(
            generation,
            "seed_policy",
            "fixed"
        );

    target.seedBase =
        jsonUint32(
            generation,
            "seed_base",
            1337u
        );

    target.weatherScale =
        jsonFloat(generation, "weather_scale", 6.0f);

    target.massScale =
        jsonFloat(generation, "mass_scale", 14.0f);

    target.erosionScale =
        jsonFloat(generation, "erosion_scale", 28.0f);

    target.detailScale =
        jsonFloat(generation, "detail_scale", 72.0f);

    target.domainWarpStrength =
        jsonFloat(
            generation,
            "domain_warp_strength",
            0.055f
        );

    target.billowStrength =
        jsonFloat(
            generation,
            "billow_strength",
            0.78f
        );

    target.worleyErosionStrength =
        jsonFloat(
            generation,
            "worley_erosion_strength",
            0.34f
        );

    target.edgeFragmentStrength =
        jsonFloat(
            generation,
            "edge_fragment_strength",
            0.52f
        );

    target.edgeFragmentScale =
        jsonFloat(
            generation,
            "edge_fragment_scale",
            56.0f
        );

    target.detachedFragmentProbability =
        jsonFloat(
            generation,
            "detached_fragment_probability",
            0.28f
        );

    target.softEdgeWidth =
        jsonFloat(
            generation,
            "soft_edge_width",
            0.10f
        );






    const json rendering =
        root.value(
            "rendering",
            json::object()
        );

    out.rendering.visualClass =
        jsonString(
            rendering,
            "visual_class",
            "terrestrial"
        );

    out.rendering.surfaceVisibility =
        jsonString(
            rendering,
            "surface_visibility",
            "visible"
        );

    out.rendering.loadSurfaceTextures =
        jsonBool(
            rendering,
            "load_surface_textures",
            true
        );

    out.rendering.baseRenderMode =
        jsonString(
            rendering,
            "base_render_mode",
            "surface_with_cloud_overlays"
        );

    out.rendering.primaryLayerId =
        jsonString(
            rendering,
            "primary_layer_id",
            ""
        );

    out.rendering.primaryLayerMustCoverSurface =
        jsonBool(
            rendering,
            "primary_layer_must_cover_surface",
            false
        );

    out.rendering.primaryLayerOpacityFloor =
        std::clamp(
            jsonFloat(
                rendering,
                "primary_layer_opacity_floor",
                0.0f
            ),
            0.0f,
            1.0f
        );

    out.rendering.maximumDynamicOverlayLayers =
        std::max(
            0,
            rendering.value(
                "maximum_dynamic_overlay_layers",
                2
            )
        );

    out.rendering.mapGenerationResolution =
        jsonIVec2(
            rendering,
            "map_generation_resolution",
            glm::ivec2(
                1024,
                512
            )
        );

    out.rendering.gameGenerationResolution =
        jsonIVec2(
            rendering,
            "game_generation_resolution",
            glm::ivec2(
                512,
                256
            )
        );

    out.rendering.overlayLayerIds.clear();

    if (rendering.contains("overlay_layer_ids") &&
        rendering["overlay_layer_ids"].is_array())
    {
        for (const json& sourceId :
            rendering["overlay_layer_ids"])
        {
            if (!sourceId.is_string())
                continue;

            out.rendering.overlayLayerIds.push_back(
                sourceId.get<std::string>()
            );
        }
    }

    const json gameUpdatePolicy =
        rendering.value(
            "game_update_policy",
            json::object()
        );

    out.rendering.gameUpdatePolicy.primaryLayer =
        jsonString(
            gameUpdatePolicy,
            "primary_layer",
            "cached_and_time_blended"
        );

    out.rendering.gameUpdatePolicy.overlayLayers =
        jsonString(
            gameUpdatePolicy,
            "overlay_layers",
            "time_sliced_one_generation_per_frame"
        );

    out.rendering.gameUpdatePolicy.farLod =
        jsonString(
            gameUpdatePolicy,
            "far_lod",
            "single_cached_primary_layer"
        );

    out.rendering.gameUpdatePolicy.offscreen =
        jsonString(
            gameUpdatePolicy,
            "offscreen",
            "do_not_update"
        );











    // Compatibility values for existing render adapters.
    if (!out.clouds.layers.empty())
    {
        float opacitySum = 0.0f;
        float windSum = 0.0f;
        float weightSum = 0.0f;

        for (const auto& layer : out.clouds.layers)
        {
            const float weight =
                std::max(
                    0.01f,
                    layer.coverage *
                    std::max(0.05f, layer.density)
                );

            opacitySum += layer.opacity * weight;

            const float meanWind =
                (
                    layer.wind.minimumSpeedMps +
                    layer.wind.maximumSpeedMps
                ) * 0.5f;

            windSum += meanWind * weight;
            weightSum += weight;
        }

        if (weightSum > 0.0f)
        {
            out.clouds.opacity =
                opacitySum / weightSum;

            // Physical velocity is converted later using body radius.
            out.clouds.longitudeDriftSpeed =
                windSum / weightSum;
        }
    }

    return true;
}



















} // namespace










bool CelestialEnvironmentProfileLibrary::loadFromRoot(
    const std::string& rootPath
)
{
    namespace fs = std::filesystem;

    m_bodyProfiles.clear();
    m_presets.clear();

    m_bodyIndex.clear();
    m_presetIndex.clear();

    m_failedCount = 0;

    const fs::path root =
        resolveProjectPath(rootPath);

    if (!fs::exists(root) ||
        !fs::is_directory(root))
    {
        std::cerr
            << "[CelestialEnvironmentProfileLibrary] root not found: "
            << root.generic_string()
            << "\n";

        return false;
    }

    // ------------------------------------------------------------
    // 1. Presets.
    // ------------------------------------------------------------
    const fs::path presetRoot =
        root / "presets";

    if (fs::exists(presetRoot) &&
        fs::is_directory(presetRoot))
    {
        for (const auto& entry :
            fs::directory_iterator(presetRoot))
        {
            if (!entry.is_regular_file() ||
                entry.path().extension() != ".json" ||
                entry.path().filename() == "MANIFEST.json")
            {
                continue;
            }

            CelestialEnvironmentProfile profile;

            if (!readEnvironmentDocument(
                    entry.path(),
                    profile
                ))
            {
                ++m_failedCount;
                continue;
            }

            if (profile.presetId.empty())
            {
                profile.presetId =
                    entry.path().stem().generic_string();
            }

            profile.loadedFromPreset = true;

            const std::string key =
                makePresetKey(
                    profile.presetId
                );

            m_presetIndex[key] =
                m_presets.size();

            m_presets.push_back(
                std::move(profile)
            );
        }
    }

    // ------------------------------------------------------------
    // 2. Optional body-specific profiles.
    // ------------------------------------------------------------
    for (const auto& systemEntry :
        fs::directory_iterator(root))
    {
        if (!systemEntry.is_directory())
            continue;

        const std::string folderName =
            systemEntry.path().filename().generic_string();

        if (normalizeEnvironmentToken(folderName) ==
            "presets")
        {
            continue;
        }

        if (normalizeEnvironmentToken(folderName) ==
            "runtime")
        {
            continue;
        }

        for (const auto& bodyEntry :
            fs::directory_iterator(systemEntry.path()))
        {
            if (!bodyEntry.is_regular_file() ||
                bodyEntry.path().extension() != ".json")
            {
                continue;
            }

            CelestialEnvironmentProfile profile;

            if (!readEnvironmentDocument(
                    bodyEntry.path(),
                    profile
                ))
            {
                ++m_failedCount;
                continue;
            }

            profile.systemFolderName =
                folderName;

            if (profile.bodyFolderName.empty())
            {
                profile.bodyFolderName =
                    bodyEntry.path().stem().generic_string();
            }

            profile.loadedBodyOverrides = true;

            const std::string key =
                makeBodyKey(
                    profile.systemFolderName,
                    profile.bodyFolderName
                );

            m_bodyIndex[key] =
                m_bodyProfiles.size();

            m_bodyProfiles.push_back(
                std::move(profile)
            );
        }
    }

    std::cout
        << "[CelestialEnvironmentProfileLibrary]"
        << " presets=" << m_presets.size()
        << " bodyProfiles=" << m_bodyProfiles.size()
        << " failed=" << m_failedCount
        << "\n";

    return !m_presets.empty();
}









std::string
CelestialEnvironmentProfileLibrary::makeBodyKey(
    const std::string& systemFolderName,
    const std::string& bodyFolderName
)
{
    return
        normalizeEnvironmentToken(systemFolderName) +
        "/" +
        normalizeEnvironmentToken(bodyFolderName);
}

std::string
CelestialEnvironmentProfileLibrary::makePresetKey(
    const std::string& presetId
)
{
    return
        normalizeEnvironmentToken(
            presetId
        );
}

const CelestialEnvironmentProfile*
CelestialEnvironmentProfileLibrary::findBodyProfile(
    const std::string& systemFolderName,
    const std::string& bodyFolderName
) const
{
    const auto it =
        m_bodyIndex.find(
            makeBodyKey(
                systemFolderName,
                bodyFolderName
            )
        );

    if (it == m_bodyIndex.end())
        return nullptr;

    return &m_bodyProfiles[it->second];
}

const CelestialEnvironmentProfile*
CelestialEnvironmentProfileLibrary::findPreset(
    const std::string& presetId
) const
{
    const auto it =
        m_presetIndex.find(
            makePresetKey(
                presetId
            )
        );

    if (it == m_presetIndex.end())
        return nullptr;

    return &m_presets[it->second];
}






CelestialEnvironmentProfile
CelestialEnvironmentProfileLibrary::resolve(
    const std::string& presetId,
    const std::string& systemFolderName,
    const std::string& bodyFolderName
) const
{
    CelestialEnvironmentProfile result;

    if (const auto* preset =
            findPreset(presetId))
    {
        result = *preset;

        result.loadedFromPreset = true;
        result.presetId = presetId;
    }

    /*
        Body profile пока считаем полной заменой только в случае,
        если он уже содержит новую v2-структуру.

        Старые legacy-файлы sol/earth.json пока не смешиваем
        с v2 автоматически, иначе можно затереть layers/zones.
    */
    const auto* body =
        findBodyProfile(
            systemFolderName,
            bodyFolderName
        );

    if (body &&
        (
            !body->clouds.layers.empty() ||
            !body->clouds.circulation.zones.empty()
        ))
    {
        result = *body;
        result.loadedBodyOverrides = true;

        if (result.presetId.empty())
            result.presetId = presetId;
    }

    result.found =
        result.loadedFromPreset ||
        result.loadedBodyOverrides;

    result.systemFolderName =
        systemFolderName;

    result.bodyFolderName =
        bodyFolderName;

    return result;
}




} // namespace world::celestial::visual