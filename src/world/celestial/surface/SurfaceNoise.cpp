#include "src/world/celestial/surface/SurfaceNoise.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{

    /*
        Constant tables (as Tobler's): 256 directions spread evenly on the
        sphere (Fibonacci) and 256 exponential magnitude steps m_i =
        exp(-s i / 255) for the strength s. A lattice point picks one of
        each with independent hash bits - stateless, no trig per corner.
    */
    struct GradientTables
    {
        glm::dvec3 directions[256];
        GradientTables()
        {
            for (int i = 0; i < 256; ++i)
            {
                const double y = 1.0 - 2.0 * (i + 0.5) / 256.0;
                const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
                const double phi = i * 2.399963229728653;
                directions[i] = glm::dvec3(r * std::cos(phi), y, r * std::sin(phi));
            }
        }
    };
    const GradientTables& gradientTables()
    {
        static const GradientTables tables;
        return tables;
    }

    struct MagnitudeTable
    {
        double s = -1.0;
        double m[256];
    };
    const double* magnitudeTable(double s)
    {
        // A handful of strengths are used; one table each per thread.
        thread_local MagnitudeTable tables[8];
        thread_local int next = 0;
        for (auto& t : tables)
            if (t.s == s)
                return t.m;
        MagnitudeTable& t = tables[next];
        next = (next + 1) % 8;
        t.s = s;
        double meanSquare = 0.0;
        for (int i = 0; i < 256; ++i)
        {
            t.m[i] = s > 0.0 ? std::exp(-s * i / 255.0) : 1.0;
            meanSquare += t.m[i] * t.m[i] / 256.0;
        }
        for (double& v : t.m)
            v /= std::sqrt(meanSquare); // unit RMS magnitude
        return t.m;
    }

    glm::dvec3 latticeGradient(std::uint64_t seed, std::int64_t x, std::int64_t y, std::int64_t z,
                               const double* magnitudes)
    {
        const std::uint64_t h = latticeHash(seed, x, y, z);
        return gradientTables().directions[h & 255u] * magnitudes[(h >> 8) & 255u];
    }

    // Octave fade-in at the LOD band limit: full from 2 x minWavelength.
    double octaveWeight(double wavelength, double minWavelength)
    {
        if (minWavelength <= 0.0)
            return 1.0;
        return std::clamp(wavelength / minWavelength - 1.0, 0.0, 1.0);
    }

    // Decorrelating offset per octave (no shared lattice between octaves).
    glm::dvec3 octaveOffset(std::uint64_t seed)
    {
        return glm::dvec3(unitFromHash(seed, 2), unitFromHash(seed, 3), unitFromHash(seed, 4)) * 1000.0;
    }
}

double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

NoiseSample gradientNoise(std::uint64_t seed, const glm::dvec3& x, double s)
{
    const double* norm = magnitudeTable(s);
    const glm::dvec3 fl = glm::floor(x);
    const glm::dvec3 f = x - fl;
    const auto ix = static_cast<std::int64_t>(fl.x);
    const auto iy = static_cast<std::int64_t>(fl.y);
    const auto iz = static_cast<std::int64_t>(fl.z);
    const glm::dvec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    const glm::dvec3 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);

    const glm::dvec3 ga = latticeGradient(seed, ix, iy, iz, norm);
    const glm::dvec3 gb = latticeGradient(seed, ix + 1, iy, iz, norm);
    const glm::dvec3 gc = latticeGradient(seed, ix, iy + 1, iz, norm);
    const glm::dvec3 gd = latticeGradient(seed, ix + 1, iy + 1, iz, norm);
    const glm::dvec3 ge = latticeGradient(seed, ix, iy, iz + 1, norm);
    const glm::dvec3 gf = latticeGradient(seed, ix + 1, iy, iz + 1, norm);
    const glm::dvec3 gg = latticeGradient(seed, ix, iy + 1, iz + 1, norm);
    const glm::dvec3 gh = latticeGradient(seed, ix + 1, iy + 1, iz + 1, norm);

    const double va = glm::dot(ga, f);
    const double vb = glm::dot(gb, f - glm::dvec3(1, 0, 0));
    const double vc = glm::dot(gc, f - glm::dvec3(0, 1, 0));
    const double vd = glm::dot(gd, f - glm::dvec3(1, 1, 0));
    const double ve = glm::dot(ge, f - glm::dvec3(0, 0, 1));
    const double vf = glm::dot(gf, f - glm::dvec3(1, 0, 1));
    const double vg = glm::dot(gg, f - glm::dvec3(0, 1, 1));
    const double vh = glm::dot(gh, f - glm::dvec3(1, 1, 1));

    NoiseSample out;
    out.value = va + u.x * (vb - va) + u.y * (vc - va) + u.z * (ve - va) + u.x * u.y * (va - vb - vc + vd) +
                u.y * u.z * (va - vc - ve + vg) + u.z * u.x * (va - vb - ve + vf) +
                u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
    const glm::dvec3 uyzx(u.y, u.z, u.x);
    const glm::dvec3 uzxy(u.z, u.x, u.y);
    out.gradient = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.z * (ge - ga) + u.x * u.y * (ga - gb - gc + gd) +
                   u.y * u.z * (ga - gc - ge + gg) + u.z * u.x * (ga - gb - ge + gf) +
                   u.x * u.y * u.z * (-ga + gb + gc - gd + ge - gf - gg + gh) +
                   du * (glm::dvec3(vb - va, vc - va, ve - va) +
                         uyzx * glm::dvec3(va - vb - vc + vd, va - vc - ve + vg, va - vb - ve + vf) +
                         uzxy * glm::dvec3(va - vb - ve + vf, va - vb - vc + vd, va - vc - ve + vg) +
                         uyzx * uzxy * (-va + vb + vc - vd + ve - vf - vg + vh));
    out.value *= 1.15;
    out.gradient *= 1.15;
    return out;
}

BandSum fbm(std::uint64_t seed, const glm::dvec3& dir, double radiusM, const OctaveSpec& spec, double macroCutoffM,
            double minWavelengthM)
{
    BandSum out;
    double amp = 1.0;
    double wavelength = spec.baseWavelengthM;
    for (int i = 0; i < spec.octaves; ++i)
    {
        const bool macro = wavelength >= macroCutoffM;
        const double w = octaveWeight(wavelength, minWavelengthM);
        if (!macro && w <= 0.0)
            break;
        const std::uint64_t s = splitMix64(seed + static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ull);
        const double n = gradientNoise(s, dir * (radiusM / wavelength) + octaveOffset(s), spec.expStrength).value;
        if (macro)
            out.macro += amp * n;
        out.total += amp * n * w;
        amp *= spec.gain;
        wavelength /= spec.lacunarity;
    }
    // Normalise by the full series (independent of where the LOD stops).
    double full = 0.0;
    double a = 1.0;
    for (int i = 0; i < spec.octaves; ++i, a *= spec.gain)
        full += a;
    out.macro /= full;
    out.total /= full;
    return out;
}

BandSum ridgedMultifractal(std::uint64_t seed, const glm::dvec3& dir, double radiusM, const OctaveSpec& spec,
                           double macroCutoffM, double minWavelengthM)
{
    BandSum out;
    double amp = 1.0;
    double full = 0.0;
    double wavelength = spec.baseWavelengthM;
    double signal = 1.0;
    for (int i = 0; i < spec.octaves; ++i)
    {
        full += amp;
        const bool macro = wavelength >= macroCutoffM;
        const double w = octaveWeight(wavelength, minWavelengthM);
        if (macro || w > 0.0)
        {
            const std::uint64_t s = splitMix64(seed + static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ull);
            const double n =
                gradientNoise(s, dir * (radiusM / wavelength) + octaveOffset(s), spec.expStrength).value;
            // Octave coupling: the next octave is weighted by this crest.
            const double weight = std::clamp(signal * 2.0, 0.0, 1.0);
            const double r = 1.0 - std::min(1.0, std::abs(n));
            signal = r * r * weight;
            if (macro)
                out.macro += amp * signal;
            out.total += amp * signal * w;
        }
        else
        {
            // Wavelengths only shrink: no later octave is needed either.
            for (int k = i + 1; k < spec.octaves; ++k)
                full += amp * std::pow(spec.gain, k - i);
            break;
        }
        amp *= spec.gain;
        wavelength /= spec.lacunarity;
    }
    out.macro /= full;
    out.total /= full;
    return out;
}

BandSum dampedFbm(std::uint64_t seed, const glm::dvec3& dir, double radiusM, const OctaveSpec& spec, double damping,
                  double macroCutoffM, double minWavelengthM)
{
    BandSum out;
    double amp = 1.0;
    double full = 0.0;
    double wavelength = spec.baseWavelengthM;
    glm::dvec3 slope(0.0);
    for (int i = 0; i < spec.octaves; ++i)
    {
        full += amp;
        const bool macro = wavelength >= macroCutoffM;
        const double w = octaveWeight(wavelength, minWavelengthM);
        if (macro || w > 0.0)
        {
            const std::uint64_t s = splitMix64(seed + static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ull);
            const NoiseSample n = gradientNoise(s, dir * (radiusM / wavelength) + octaveOffset(s), spec.expStrength);
            // Slope on the sphere (tangential part), accumulated unfaded.
            slope += (n.gradient - dir * glm::dot(n.gradient, dir)) * amp;
            const double term = amp * n.value / (1.0 + damping * glm::dot(slope, slope));
            if (macro)
                out.macro += term;
            out.total += term * w;
        }
        else
        {
            for (int k = i + 1; k < spec.octaves; ++k)
                full += amp * std::pow(spec.gain, k - i);
            break;
        }
        amp *= spec.gain;
        wavelength /= spec.lacunarity;
    }
    out.macro /= full;
    out.total /= full;
    return out;
}
} // namespace world::celestial::surface
