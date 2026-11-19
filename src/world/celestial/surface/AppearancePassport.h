#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/world/celestial/surface/SurfaceField.h"

namespace world::celestial::surface
{
/*
    Appearance passport of a famous body: what keeps a procedural body
    recognisable as itself. The generator still makes the relief, craters
    and pattern from the physical descriptor; the passport only states
    observed facts the physics chain does not carry (today: the mean surface
    colour) and whether the body is drawn procedurally instead of its curated
    maps.

    Data: assets/data/celestial/appearance_passports.json (source of truth in
    src/assets/data/celestial/), keyed by the generated body id
    ("system_0.Sol.Марс.Фобос"). Looked up from the working directory and
    its two parents, like the other runtime data.

    meanAlbedo is LINEAR reflectance (the field's appearance space, before
    the sRGB encode). The field's own mean colour over the sphere is scaled
    per channel to it, so the generator's variation stays and the average
    matches the real body.
*/
/*
    The look of a body in the Modern (cinematic) style - render-only facts
    the shading palette cannot derive (docs/FORTUNA_LEXIE.md: Lexie). All
    colours LINEAR. Absent block = the style's generic palette (unchanged).
    Never part of the surface identity / map bake: shading uniforms only.
*/
struct BodyLook
{
    // the sea: shallow -> deep by the DEPTH (the coast channel is the height relative to the sea level, km);
    // glint strength (1 = generic) and sharpness
    glm::vec3 seaShallow {0.010f, 0.055f, 0.15f};
    glm::vec3 seaDeep {0.003f, 0.018f, 0.075f};
    float shelfDepthKm = 180.0f;   // sea depth (km below the sea level) where the shallow colour turns deep
    float glint = 1.0f;
    float glintPower = 60.0f;
    // the natural zones' land palette
    glm::vec3 forest {0.028f, 0.065f, 0.016f};
    glm::vec3 plains {0.12f, 0.17f, 0.045f};
    glm::vec3 dry {0.42f, 0.29f, 0.13f};
    // night: glowing sea patches (bioluminescent "milky seas"): colour x strength, share of the sea covered
    glm::vec3 nightSeaGlow {0.0f};
    float nightSeaCover = 0.0f;
    // night airglow layer (colour x strength per planet radius of path) and the auroral ovals
    glm::vec3 airglow {0.0f};
    glm::vec3 aurora {0.0f};        // the green core
    glm::vec3 auroraTop {0.0f};     // the red tops
    float auroraColatDeg = 20.0f;   // the oval's distance from the magnetic pole
    float auroraWidthDeg = 4.0f;
    float magneticTiltDeg = 0.0f;   // magnetic pole off the spin axis
    // the sky's scattering colour (overrides the composition class when set)
    bool hasScatter = false;
    glm::vec3 scatter {0.17f, 0.40f, 1.0f};
};

struct AppearancePassport
{
    std::string bodyId;
    bool procedural = false;        // draw from the generator, not the curated / cached maps
    bool hasMeanAlbedo = false;
    glm::vec3 meanAlbedo {0.0f};    // linear
    std::string reference;          // where the colour comes from
    bool hasLook = false;
    BodyLook look;
};

// nullptr when the body has no passport. Thread-safe (loaded once).
const AppearancePassport* findAppearancePassport(const std::string& bodyId);

// The field with the passport's colour calibration applied (or the field
// itself when the body has no colour in its passport).
std::unique_ptr<SurfaceField> applyAppearancePassport(std::unique_ptr<SurfaceField> field,
                                                      const std::string& bodyId);

// Mean linear albedo of a field over the sphere (coarse, deterministic).
glm::vec3 meanFieldAlbedo(const SurfaceField& field);
} // namespace world::celestial::surface
