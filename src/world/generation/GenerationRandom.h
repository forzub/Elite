#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace world::generation
{

/*
    Deterministic seed derivation and random streams for the universe
    generator.

    Every random quantity is drawn from a stream derived from its owner's seed
    and a stable tag (and index), never from a shared sequential generator:

        UniverseSeed -> GalacticCellSeed -> SystemSeed -> StarSeed
                                                       -> PlanetSeed -> MoonSeed
                                                       -> ReservoirSeed

    A new draw in one stream (for example the atmosphere) never shifts any
    other stream (for example the moons), and the result does not depend on
    generation order or on which other systems were generated.

    The standard library distributions are not used: their algorithms are
    implementation-defined, so the same seed could give different worlds on
    different compilers. All sampling below is explicit.
*/

inline std::uint64_t splitMix64(std::uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

inline std::uint64_t hashTag(std::string_view tag)
{
    std::uint64_t h = 0xCBF29CE484222325ull; // FNV-1a 64
    for (const char c : tag)
    {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001B3ull;
    }
    return h;
}

// Child seed of `parent` for the role `tag` (and an index inside the role).
inline std::uint64_t deriveSeed(
    std::uint64_t parent, std::string_view tag, std::uint64_t index = 0)
{
    return splitMix64(
        splitMix64(parent ^ hashTag(tag)) + splitMix64(index + 0x632BE59BD9B4E019ull));
}

class RandomStream
{
public:
    explicit RandomStream(std::uint64_t seed)
    {
        std::uint64_t x = seed;
        for (auto& s : m_state)
        {
            x = splitMix64(x);
            s = x;
        }
    }

    RandomStream(std::uint64_t parent, std::string_view tag, std::uint64_t index = 0)
        : RandomStream(deriveSeed(parent, tag, index))
    {
    }

    // xoshiro256**
    std::uint64_t next()
    {
        const std::uint64_t result = rotl(m_state[1] * 5, 7) * 9;
        const std::uint64_t t = m_state[1] << 17;
        m_state[2] ^= m_state[0];
        m_state[3] ^= m_state[1];
        m_state[1] ^= m_state[2];
        m_state[0] ^= m_state[3];
        m_state[2] ^= t;
        m_state[3] = rotl(m_state[3], 45);
        return result;
    }

    // [0, 1)
    double uniform()
    {
        return static_cast<double>(next() >> 11) * 0x1.0p-53;
    }

    double uniform(double lo, double hi)
    {
        return lo + (hi - lo) * uniform();
    }

    // (0, 1]: safe for log().
    double uniformOpen()
    {
        return 1.0 - uniform();
    }

    bool chance(double probability)
    {
        return uniform() < probability;
    }

    int uniformInt(int lo, int hiInclusive)
    {
        const auto span = static_cast<std::uint64_t>(hiInclusive - lo + 1);
        return lo + static_cast<int>(next() % span);
    }

    // Box-Muller; one value per call keeps the stream layout simple.
    double normal(double mean = 0.0, double sigma = 1.0)
    {
        const double u1 = uniformOpen();
        const double u2 = uniform();
        return mean + sigma * std::sqrt(-2.0 * std::log(u1)) *
            std::cos(6.283185307179586 * u2);
    }

    double truncatedNormal(double mean, double sigma, double lo, double hi)
    {
        for (int i = 0; i < 64; ++i)
        {
            const double v = normal(mean, sigma);
            if (v >= lo && v <= hi)
                return v;
        }
        return mean < lo ? lo : (mean > hi ? hi : mean);
    }

    // log10-normal: median 10^mu10, scatter sigma10 dex.
    double logNormal10(double median, double sigmaDex)
    {
        return median * std::pow(10.0, normal(0.0, sigmaDex));
    }

    double logUniform(double lo, double hi)
    {
        return lo * std::pow(hi / lo, uniform());
    }

    // dN/dx ~ x^alpha on [lo, hi].
    double powerLaw(double alpha, double lo, double hi)
    {
        const double u = uniform();
        if (std::abs(alpha + 1.0) < 1.0e-9)
            return lo * std::pow(hi / lo, u);
        const double a1 = alpha + 1.0;
        const double l = std::pow(lo, a1);
        const double h = std::pow(hi, a1);
        return std::pow(l + (h - l) * u, 1.0 / a1);
    }

    // Rayleigh with mode sigma (orbital eccentricities / inclinations).
    double rayleigh(double sigma)
    {
        return sigma * std::sqrt(-2.0 * std::log(uniformOpen()));
    }

    // Knuth for small means; normal approximation beyond.
    int poisson(double mean)
    {
        if (mean <= 0.0)
            return 0;
        if (mean > 30.0)
        {
            const double v = normal(mean, std::sqrt(mean));
            return v < 0.0 ? 0 : static_cast<int>(v + 0.5);
        }
        const double l = std::exp(-mean);
        int k = 0;
        double p = 1.0;
        do
        {
            ++k;
            p *= uniform();
        } while (p > l);
        return k - 1;
    }

    // Isotropic orientation angle: cos(i) uniform in [-1, 1].
    double isotropicInclinationRad()
    {
        return std::acos(uniform(-1.0, 1.0));
    }

    double angleRad()
    {
        return uniform(0.0, 6.283185307179586);
    }

private:
    static std::uint64_t rotl(std::uint64_t x, int k)
    {
        return (x << k) | (x >> (64 - k));
    }

    std::uint64_t m_state[4] {};
};

} // namespace world::generation
