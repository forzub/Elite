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
struct AppearancePassport
{
    std::string bodyId;
    bool procedural = false;        // draw from the generator, not the curated / cached maps
    bool hasMeanAlbedo = false;
    glm::vec3 meanAlbedo {0.0f};    // linear
    std::string reference;          // where the colour comes from
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
