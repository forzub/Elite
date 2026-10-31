#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/surface/CloudWeatherField.h"
#include "src/world/celestial/surface/SurfaceField.h"

namespace world::celestial::surface
{
/*
    Far representation of a body: its SurfaceField sampled on a cube map
    (GL face convention) at a resolution chosen by the renderer from the
    body's size on screen. It is a cache of the continuous field, not a new
    world: every texel is SurfaceField::sample(dir, 2 x texel) - the macro
    geography (coastline, ice, landmarks) is the same at every face size,
    a larger face only resolves more of it.

    Per texel (body-fixed direction):
      albedo  RGBA8  rgb = surface colour (sqrt-encoded), a = 255 water
      normal  RGBA8  xyz = relief normal (body frame, *0.5+0.5), a = emission
      relief  RG16F  r = displayed height (km), g = coast value (km)
*/
struct GlobeFaces
{
    int size = 0;
    std::array<std::vector<std::uint8_t>, 6> albedo;
    std::array<std::vector<std::uint8_t>, 6> normal;
    std::array<std::vector<std::uint16_t>, 6> relief; // half floats, 2 per texel
    glm::vec3 meanAlbedo {0.5f};
    std::array<double, 6> faceSeconds {}; // CPU time per face

    std::size_t bytes() const
    {
        return static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 6u * 12u;
    }
};

// GL cube map texel direction: face 0..5 = +X -X +Y -Y +Z -Z, (s, t) in
// [-1, 1] with t growing down the uploaded rows.
glm::dvec3 cubeMapDirection(int face, double s, double t);
// Inverse: face and (s, t) of a direction.
int cubeMapFace(const glm::dvec3& dir, double& s, double& t);

// One face (independent of the others, any thread). Returns false when
// cancelled.
bool bakeGlobeFace(const SurfaceField& field, int size, int face, GlobeFaces& out,
                   const std::atomic<bool>* cancel = nullptr);
void finishGlobeBake(GlobeFaces& out); // mean albedo

// Cloud cover of one layer at a world time: R8 per texel.
struct CloudFaces
{
    int size = 0;
    double worldTimeSeconds = 0.0;
    std::array<std::vector<std::uint8_t>, 6> coverage;
};
void bakeCloudFace(const CloudWeatherField& clouds, std::size_t layer, double worldTimeSeconds, int size, int face,
                   CloudFaces& out);

std::uint16_t toHalf(float value);
} // namespace world::celestial::surface
