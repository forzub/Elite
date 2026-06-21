#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial::visual
{

// -------------------------------------------------------------------------------------
// Visual class of a celestial body.
// This is not strict astrophysics; this is a rendering archetype.
// One class can later be mapped to different physical body types.
// -------------------------------------------------------------------------------------
enum class CelestialVisualClass
{
    RockyBarren,   // Moon, Mercury, barren rocky bodies
    RockyIce,      // Europa-like, icy moons
    Terrestrial,   // Earth-like solid world
    OceanWorld,    // Mostly ocean / water-dominated
    DesertWorld,   // Mars-like or dune-dominated
    Venusian,      // Dense-atmosphere, cloudy, muted solid body
    GasGiant,      // Jupiter / Saturn style
    IceGiant,      // Uranus / Neptune style
    LavaWorld,     // Volcanic / molten / crack-glow world
    AsteroidRock,  // Irregular rocky minor body
    CometaryIce,   // Dirty ice body
    RingSystem,    // Reserved for future ring texture pipeline
    StellarSurface // Reserved for stars
};




enum class CelestialSurfaceSourceKind
{
    // Authoritative imported maps / meshes.
    // Used for Solar System bodies.
    ImportedRealData,

    // Procedural generation for unknown / fictional / poorly known worlds.
    ProceduralFromClassSeed,

    // Explicit temporary placeholder.
    // This must never be silently treated as authoritative.
    PlaceholderKnownBodyProcedural
};

inline const char* toString(CelestialSurfaceSourceKind k)
{
    switch (k)
    {
        case CelestialSurfaceSourceKind::ImportedRealData:
            return "ImportedRealData";

        case CelestialSurfaceSourceKind::ProceduralFromClassSeed:
            return "ProceduralFromClassSeed";

        case CelestialSurfaceSourceKind::PlaceholderKnownBodyProcedural:
            return "PlaceholderKnownBodyProcedural";

        default:
            return "PlaceholderKnownBodyProcedural";
    }
}

inline bool fromString(
    const std::string& s,
    CelestialSurfaceSourceKind& out
)
{
    if (s == "ImportedRealData" || s == "ImportedTextureSet")
    {
        out = CelestialSurfaceSourceKind::ImportedRealData;
        return true;
    }

    if (s == "ProceduralFromClassSeed" || s == "ProceduralGeneric")
    {
        out = CelestialSurfaceSourceKind::ProceduralFromClassSeed;
        return true;
    }

    if (s == "PlaceholderKnownBodyProcedural" ||
        s == "KnownBodyProcedural" ||
        s == "EarthCoastlineMask")
    {
        out = CelestialSurfaceSourceKind::PlaceholderKnownBodyProcedural;
        return true;
    }

    return false;
}














// -------------------------------------------------------------------------------------
// Subtle, mostly-monochrome color styling.
// The idea is:
//   - keep worlds visually restrained;
//   - stay close to grayscale;
//   - still preserve class readability via a small tint.
// -------------------------------------------------------------------------------------
struct CelestialColorStyle
{
    glm::vec3 tint {1.0f, 1.0f, 1.0f};

    // 0.0 = pure grayscale
    // 1.0 = fully tinted
    float saturation = 0.10f;

    float contrast = 1.0f;
    float brightness = 1.0f;
};

// -------------------------------------------------------------------------------------
// Surface generation parameters.
// Some fields are more relevant for one class than for another.
// That is normal.
// -------------------------------------------------------------------------------------
struct SurfaceGenerationParams
{
    uint32_t seed = 0;

    // Generic noise scales.
    float macroNoiseScale = 1.0f;
    float detailNoiseScale = 1.0f;

    // Generic shape controls.
    float roughness = 0.5f;
    float ridgeStrength = 0.0f;
    float erosion = 0.0f;

    // Impact / crater-driven surface detail.
    float craterDensity = 0.0f;
    float craterStrength = 0.0f;

    // Additional shape controls for less "soap-smooth" planets.
    float craterSmallDensity = 0.0f;
    float craterLargeDensity = 0.0f;

    float continentFrequency = 1.0f;
    float continentWarp = 0.0f;

    float mountainAmount = 0.0f;
    float canyonAmount = 0.0f;

    float albedoVariation = 0.0f;

    // World-type controls.
    float oceanLevel = 0.0f;
    float iceCoverage = 0.0f;
    float desertness = 0.0f;
    float lavaCoverage = 0.0f;

    // Gas / ice giant banding.
    float bandingStrength = 0.0f;
    float stormStrength = 0.0f;
};

// -------------------------------------------------------------------------------------
// Output resolution profile.
// Keep all 3 base maps aligned in size.
// Clouds/emission will be added later, but the structure is ready.
// -------------------------------------------------------------------------------------
struct TextureGenerationProfile
{
    // Fallback / minimum explicit size.
    int width = 2048;
    int height = 1024;

    // If true, the baker chooses the final global map size
    // from body radius and target meters-per-texel.
    bool autoFromRadius = false;

    // Upper cap for the global texture.
    int maxWidth = 8192;
    int maxHeight = 4096;

    // Global map target density.
    // This is for the whole-planet texture, not for very low altitude.
    double targetMetersPerTexel = 4000.0;

    // Future near-surface detail target.
    // Not used by the current baker yet.
    double detailTargetMetersPerTexel = 250.0;

    // Height where the surface should still read well enough.
    // Also not fully used yet, but this belongs in the architecture now.
    double minMeaningfulAltitudeKm = 300.0;
};

// -------------------------------------------------------------------------------------
// Result paths written by the generator.
// Right now we write .tga because it is simple and already loadable by stb_image.
// Later we can switch to .png in one place only.
// -------------------------------------------------------------------------------------
struct GeneratedTextureSet
{
    std::string albedoPath;
    std::string normalPath;
    std::string materialPath;

    std::string cloudsPath;
    std::string emissionPath;

    std::string shapeModelPath;

    std::string previewPath;
    std::string previewFlatPath;
    std::string metaPath;
};

// -------------------------------------------------------------------------------------
// Full body visual descriptor.
// This is the "single source of truth" for the procedural / generated texture pipeline.
// It is verbose by design.
// -------------------------------------------------------------------------------------
struct CelestialBodyVisualDescriptor
{
    // Grouping / folder layout.
    std::string systemFolderName;
    std::string bodyFolderName;

    // Human-readable / source identity.
    std::string displayName;
    std::string sourceSystemId;
    std::string sourceBodyId;

    // Physical size used by automatic texture resolution selection.
    // 0.0 means "unknown / do not auto-compute from radius".
    double referenceRadiusKm = 0.0;

    // Optional notes for us, useful in meta.json and debugging.
    std::string notes;

    CelestialVisualClass visualClass =
        CelestialVisualClass::RockyBarren;


    CelestialSurfaceSourceKind surfaceSourceKind =
        CelestialSurfaceSourceKind::PlaceholderKnownBodyProcedural;

    // Optional data source for surface generation.
    // Example:
    //   src/assets/data/planets/earth_coastlines.json
    std::string surfaceReferencePath;



    // Source catalog descriptor.
    // For Sol bodies this points to:
    // src/assets/data/celestial/source_catalog/sol/<body>.source.json
    std::string sourceCatalogPath;

    // Data confidence marker.
    // Examples:
    //   authoritative
    //   procedural
    //   placeholder
    std::string dataConfidence = "placeholder";




    // Architectural flags.
    bool hasAtmosphere = false;
    bool hasCloudLayer = false; // reserved for the next stage
    bool hasEmission = false;   // reserved for night lights / lava emission

    CelestialColorStyle colorStyle;
    SurfaceGenerationParams surface;
    TextureGenerationProfile textureProfile;

    GeneratedTextureSet textures;
};

// -------------------------------------------------------------------------------------
// Preset record loaded from visual_presets.json.
// Body descriptors can inherit from these and override fields selectively.
// -------------------------------------------------------------------------------------
struct CelestialVisualPreset
{
    std::string presetId;
    CelestialBodyVisualDescriptor descriptor;
};

// -------------------------------------------------------------------------------------
// String conversion helpers.
// -------------------------------------------------------------------------------------
inline const char* toString(CelestialVisualClass c)
{
    switch (c)
    {
        case CelestialVisualClass::RockyBarren:   return "RockyBarren";
        case CelestialVisualClass::RockyIce:      return "RockyIce";
        case CelestialVisualClass::Terrestrial:   return "Terrestrial";
        case CelestialVisualClass::OceanWorld:    return "OceanWorld";
        case CelestialVisualClass::DesertWorld:   return "DesertWorld";
        case CelestialVisualClass::Venusian:      return "Venusian";
        case CelestialVisualClass::GasGiant:      return "GasGiant";
        case CelestialVisualClass::IceGiant:      return "IceGiant";
        case CelestialVisualClass::LavaWorld:     return "LavaWorld";
        case CelestialVisualClass::AsteroidRock:  return "AsteroidRock";
        case CelestialVisualClass::CometaryIce:   return "CometaryIce";
        case CelestialVisualClass::RingSystem:    return "RingSystem";
        case CelestialVisualClass::StellarSurface:return "StellarSurface";
        default:                                  return "RockyBarren";
    }
}

inline bool fromString(
    const std::string& s,
    CelestialVisualClass& out
)
{
    if (s == "RockyBarren")    { out = CelestialVisualClass::RockyBarren; return true; }
    if (s == "RockyIce")       { out = CelestialVisualClass::RockyIce; return true; }
    if (s == "Terrestrial")    { out = CelestialVisualClass::Terrestrial; return true; }
    if (s == "OceanWorld")     { out = CelestialVisualClass::OceanWorld; return true; }
    if (s == "DesertWorld")    { out = CelestialVisualClass::DesertWorld; return true; }
    if (s == "Venusian")       { out = CelestialVisualClass::Venusian; return true; }
    if (s == "GasGiant")       { out = CelestialVisualClass::GasGiant; return true; }
    if (s == "IceGiant")       { out = CelestialVisualClass::IceGiant; return true; }
    if (s == "LavaWorld")      { out = CelestialVisualClass::LavaWorld; return true; }
    if (s == "AsteroidRock")   { out = CelestialVisualClass::AsteroidRock; return true; }
    if (s == "CometaryIce")    { out = CelestialVisualClass::CometaryIce; return true; }
    if (s == "RingSystem")     { out = CelestialVisualClass::RingSystem; return true; }
    if (s == "StellarSurface") { out = CelestialVisualClass::StellarSurface; return true; }

    return false;
}

} // namespace world::celestial::visual