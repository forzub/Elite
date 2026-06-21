#include "CelestialVisualLibrary.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <nlohmann/json.hpp>

namespace world::celestial::visual
{

namespace
{

using json = nlohmann::json;

glm::vec3 readVec3(const json& j, const glm::vec3& fallback)
{
    if (!j.is_array() || j.size() != 3)
        return fallback;

    return glm::vec3(
        j[0].get<float>(),
        j[1].get<float>(),
        j[2].get<float>()
    );
}




void applyCommonOverrides(
    const json& src,
    CelestialBodyVisualDescriptor& out
)
{
    if (src.contains("display_name"))
        out.displayName = src.value("display_name", out.displayName);

    if (src.contains("source_system_id"))
        out.sourceSystemId = src.value("source_system_id", out.sourceSystemId);

    if (src.contains("source_body_id"))
        out.sourceBodyId = src.value("source_body_id", out.sourceBodyId);
    
        // Physical radius is intentionally NOT loaded from body_visuals.
    // The only authoritative source for body size is:
    // src/assets/data/star_atlas/system_details.json
    //
    // CelestialTextureBaker enriches descriptors from StarAtlasDatabase before baking.
    // Keeping radius out of body_visuals prevents duplicated physical data.
    // if (src.contains("reference_radius_km"))
    //     out.referenceRadiusKm = src.value("reference_radius_km", out.referenceRadiusKm);

    if (src.contains("notes"))
        out.notes = src.value("notes", out.notes);

    if (src.contains("system_folder_name"))
        out.systemFolderName = src.value("system_folder_name", out.systemFolderName);

    if (src.contains("body_folder_name"))
        out.bodyFolderName = src.value("body_folder_name", out.bodyFolderName);

    if (src.contains("visual_class"))
    {
        CelestialVisualClass c;
        if (fromString(src.value("visual_class", ""), c))
            out.visualClass = c;
    }


    if (src.contains("surface_source_kind"))
    {
        CelestialSurfaceSourceKind k;
        if (fromString(src.value("surface_source_kind", ""), k))
            out.surfaceSourceKind = k;
    }

    if (src.contains("surface_reference_path"))
    {
        out.surfaceReferencePath =
            src.value("surface_reference_path", out.surfaceReferencePath);
    }



    if (src.contains("source_catalog_path"))
    {
        out.sourceCatalogPath =
            src.value("source_catalog_path", out.sourceCatalogPath);
    }

    if (src.contains("data_confidence"))
    {
        out.dataConfidence =
            src.value("data_confidence", out.dataConfidence);
    }









    if (src.contains("has_atmosphere"))
        out.hasAtmosphere = src.value("has_atmosphere", out.hasAtmosphere);

    if (src.contains("has_cloud_layer"))
        out.hasCloudLayer = src.value("has_cloud_layer", out.hasCloudLayer);

    if (src.contains("has_emission"))
        out.hasEmission = src.value("has_emission", out.hasEmission);

    if (src.contains("color_style") && src["color_style"].is_object())
    {
        const auto& c = src["color_style"];

        if (c.contains("tint"))
            out.colorStyle.tint = readVec3(c["tint"], out.colorStyle.tint);

        if (c.contains("saturation"))
            out.colorStyle.saturation = c.value("saturation", out.colorStyle.saturation);

        if (c.contains("contrast"))
            out.colorStyle.contrast = c.value("contrast", out.colorStyle.contrast);

        if (c.contains("brightness"))
            out.colorStyle.brightness = c.value("brightness", out.colorStyle.brightness);
    }

    if (src.contains("surface") && src["surface"].is_object())
    {
        const auto& s = src["surface"];

                out.surface.seed = s.value("seed", out.surface.seed);
        out.surface.macroNoiseScale = s.value("macro_noise_scale", out.surface.macroNoiseScale);
        out.surface.detailNoiseScale = s.value("detail_noise_scale", out.surface.detailNoiseScale);
        out.surface.roughness = s.value("roughness", out.surface.roughness);
        out.surface.ridgeStrength = s.value("ridge_strength", out.surface.ridgeStrength);
        out.surface.erosion = s.value("erosion", out.surface.erosion);
        out.surface.craterDensity = s.value("crater_density", out.surface.craterDensity);
        out.surface.craterStrength = s.value("crater_strength", out.surface.craterStrength);

        out.surface.craterSmallDensity = s.value("crater_small_density", out.surface.craterSmallDensity);
        out.surface.craterLargeDensity = s.value("crater_large_density", out.surface.craterLargeDensity);

        out.surface.continentFrequency = s.value("continent_frequency", out.surface.continentFrequency);
        out.surface.continentWarp = s.value("continent_warp", out.surface.continentWarp);

        out.surface.mountainAmount = s.value("mountain_amount", out.surface.mountainAmount);
        out.surface.canyonAmount = s.value("canyon_amount", out.surface.canyonAmount);

        out.surface.albedoVariation = s.value("albedo_variation", out.surface.albedoVariation);

        out.surface.oceanLevel = s.value("ocean_level", out.surface.oceanLevel);
        out.surface.iceCoverage = s.value("ice_coverage", out.surface.iceCoverage);
        out.surface.desertness = s.value("desertness", out.surface.desertness);
        out.surface.lavaCoverage = s.value("lava_coverage", out.surface.lavaCoverage);
        out.surface.bandingStrength = s.value("banding_strength", out.surface.bandingStrength);
        out.surface.stormStrength = s.value("storm_strength", out.surface.stormStrength);
    }

        if (src.contains("texture_profile") && src["texture_profile"].is_object())
    {
        const auto& t = src["texture_profile"];

        out.textureProfile.width = t.value("width", out.textureProfile.width);
        out.textureProfile.height = t.value("height", out.textureProfile.height);

        out.textureProfile.autoFromRadius =
            t.value("auto_from_radius", out.textureProfile.autoFromRadius);

        out.textureProfile.maxWidth =
            t.value("max_width", out.textureProfile.maxWidth);

        out.textureProfile.maxHeight =
            t.value("max_height", out.textureProfile.maxHeight);

        out.textureProfile.targetMetersPerTexel =
            t.value("target_meters_per_texel", out.textureProfile.targetMetersPerTexel);

        out.textureProfile.detailTargetMetersPerTexel =
            t.value("detail_target_meters_per_texel", out.textureProfile.detailTargetMetersPerTexel);

        out.textureProfile.minMeaningfulAltitudeKm =
            t.value("min_meaningful_altitude_km", out.textureProfile.minMeaningfulAltitudeKm);
    }
}



















json loadJson(const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open JSON: " + path.string());

    json j;
    f >> j;
    return j;
}

} // namespace

bool CelestialVisualLibrary::load(
    const std::string& presetsPath,
    const std::string& bodyVisualsRoot
)
{
    m_presets.clear();
    m_bodies.clear();

    try
    {
        // -------------------------------------------------------------------------
        // Load presets
        // -------------------------------------------------------------------------
        {
            const json root = loadJson(presetsPath);

            if (!root.contains("presets") || !root["presets"].is_array())
            {
                std::cerr << "[CelestialVisualLibrary] presets array is missing\n";
                return false;
            }

            for (const auto& item : root["presets"])
            {
                if (!item.is_object())
                    continue;

                CelestialVisualPreset preset;
                preset.presetId = item.value("preset_id", "");

                applyCommonOverrides(item, preset.descriptor);

                if (preset.presetId.empty())
                    continue;

                m_presets.emplace(preset.presetId, std::move(preset));
            }
        }

        // -------------------------------------------------------------------------
        // Load body descriptors recursively, grouped by system folders
        // -------------------------------------------------------------------------
        namespace fs = std::filesystem;

        for (const auto& entry : fs::recursive_directory_iterator(bodyVisualsRoot))
        {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() != ".json")
                continue;

            const json root = loadJson(entry.path());

            CelestialBodyVisualDescriptor desc;

            const std::string presetId =
                root.value("preset_id", "");

            if (!presetId.empty())
            {
                auto it = m_presets.find(presetId);
                if (it != m_presets.end())
                    desc = it->second.descriptor;
            }

            applyCommonOverrides(root, desc);

            if (desc.systemFolderName.empty())
            {
                auto rel = fs::relative(entry.path().parent_path(), bodyVisualsRoot);
                if (!rel.empty())
                    desc.systemFolderName = rel.generic_string();
            }

            if (desc.bodyFolderName.empty())
                desc.bodyFolderName = entry.path().stem().string();

            if (desc.displayName.empty())
                desc.displayName = desc.bodyFolderName;

            if (desc.sourceBodyId.empty())
                desc.sourceBodyId = desc.bodyFolderName;

            if (desc.sourceSystemId.empty())
                desc.sourceSystemId = desc.systemFolderName;

            m_bodies.push_back(std::move(desc));
        }



        std::sort(
            m_bodies.begin(),
            m_bodies.end(),
            [](const CelestialBodyVisualDescriptor& a,
               const CelestialBodyVisualDescriptor& b)
            {
                if (a.systemFolderName != b.systemFolderName)
                    return a.systemFolderName < b.systemFolderName;

                return a.bodyFolderName < b.bodyFolderName;
            }
        );



        std::cout
            << "[CelestialVisualLibrary] presets=" << m_presets.size()
            << " bodies=" << m_bodies.size()
            << "\n";

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[CelestialVisualLibrary] load failed: "
            << e.what() << "\n";
        return false;
    }
}

} // namespace world::celestial::visual