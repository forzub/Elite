#pragma once

// The conservative bounding radius of a body from its REAL collision geometry (audit 8e3c49007 NAV-2: the avoidance
// used 30 m for every ship and max(10, length / 2) for its own hull - a Cobra is wider than long, half the catalog is
// longer than 60 m). One definition for every consumer: the radar truth (the track's boundingRadiusMeters), our own
// hull in the reflex, later debris / cargo / wrecks. Works on any volume list with center / halfSize / destroyed
// (damage::HitVolume, DebugHitVolumeSnapshot): the largest |center| + |halfSize| over the live volumes - the sphere
// round the body's origin that contains every live box whatever its orientation. A lost (destroyed) volume no longer
// counts: a torn-off part shrinks the body.
#include <algorithm>

#include <glm/glm.hpp>

namespace game::damage
{
template <class Volumes>
double hitVolumesBoundingRadius(const Volumes& volumes, double fallbackMeters = 0.0)
{
    double r = 0.0;
    for (const auto& v : volumes)
        if (!v.destroyed)
            r = std::max(r, static_cast<double>(glm::length(glm::vec3(v.center)) + glm::length(glm::vec3(v.halfSize))));
    return r > 0.0 ? r : fallbackMeters;
}

// a hull known only by its overall dimensions (a descriptor, a presentation-only traffic hull): the half diagonal of
// the box - every point of the hull is inside it
inline double dimensionsBoundingRadius(double length, double width, double height)
{
    return 0.5 * std::sqrt(length * length + width * width + height * height);
}
} // namespace game::damage
