#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace world::celestial::surface
{
/*
    Stateless noise on the sphere for the surface generator.

    gradientNoise: 3D gradient noise (quintic, analytic derivative, after
    Inigo Quilez) whose lattice gradients have EXPONENTIALLY distributed
    magnitudes (Parberry, Tobler: "exponential gradients"): most lattice
    points are calm and a few are steep, which gives broad quiet regions and
    rare strong relief instead of uniform "sandpaper". The lattice is hashed
    per point (no permutation tables, no period, no order dependence).

    The octave sums are evaluated in continuous planet-local 3D coordinates
    (unit direction x radius / wavelength), never per patch, so patches meet
    without seams. Every sum is split into two bands:

        macro: octaves with wavelength >= macroCutoffM - the LOD-independent
               field (coastlines, landmarks) - always evaluated in full;
        total: all octaves down to minWavelengthM (the LOD band limit; the
               last octave fades in), which a patch displays.

    The cross-octave state (ridged weight, derivative damping) is driven by
    the unfaded octaves, so a finer LOD only APPENDS octaves: the first k
    terms of a sum are identical at every LOD.
*/
struct NoiseSample
{
    double value = 0.0;           // about [-1, 1]
    glm::dvec3 gradient {0.0};    // d value / d p
};

// expStrength s: gradient magnitude m = exp(-s * u), u uniform (Tobler mu =
// exp(s / 256)); 0 = classic Perlin. The output is normalised to unit RMS
// gradient magnitude.
NoiseSample gradientNoise(std::uint64_t seed, const glm::dvec3& p, double expStrength);

struct OctaveSpec
{
    double baseWavelengthM = 1.0e6;
    double lacunarity = 1.92;     // not 2: no lattice alignment (Scape)
    double gain = 0.5;            // amplitude ratio per octave
    int octaves = 12;
    double expStrength = 5.0;
};

struct BandSum
{
    double macro = 0.0;
    double total = 0.0;
};

// Plain fBm, about [-1, 1].
BandSum fbm(std::uint64_t seed, const glm::dvec3& dir, double radiusM, const OctaveSpec& spec,
            double macroCutoffM, double minWavelengthM);

// Ridged multifractal with octave weight coupling (Musgrave / Scape
// "multiturbulence"): sharp crests, smooth valleys, about [0, 1].
BandSum ridgedMultifractal(std::uint64_t seed, const glm::dvec3& dir, double radiusM, const OctaveSpec& spec,
                           double macroCutoffM, double minWavelengthM);

// Derivative-damped fBm (Quilez / Scape "iq turbulence"): octaves are
// suppressed where the accumulated slope is high - eroded-looking flanks
// and calm basins. damping 0 = plain fBm. About [-1, 1].
BandSum dampedFbm(std::uint64_t seed, const glm::dvec3& dir, double radiusM, const OctaveSpec& spec,
                  double damping, double macroCutoffM, double minWavelengthM);

double smoothstep(double edge0, double edge1, double x);
} // namespace world::celestial::surface
