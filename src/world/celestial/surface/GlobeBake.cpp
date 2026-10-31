#include "src/world/celestial/surface/GlobeBake.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace world::celestial::surface
{
glm::dvec3 cubeMapDirection(int face, double s, double t)
{
    glm::dvec3 d;
    switch (face)
    {
        case 0: d = {1.0, -t, -s}; break;
        case 1: d = {-1.0, -t, s}; break;
        case 2: d = {s, 1.0, t}; break;
        case 3: d = {s, -1.0, -t}; break;
        case 4: d = {s, -t, 1.0}; break;
        default: d = {-s, -t, -1.0}; break;
    }
    return glm::normalize(d);
}

int cubeMapFace(const glm::dvec3& d, double& s, double& t)
{
    const glm::dvec3 a = glm::abs(d);
    if (a.x >= a.y && a.x >= a.z)
    {
        if (d.x > 0.0) { s = -d.z / a.x; t = -d.y / a.x; return 0; }
        s = d.z / a.x; t = -d.y / a.x; return 1;
    }
    if (a.y >= a.z)
    {
        if (d.y > 0.0) { s = d.x / a.y; t = d.z / a.y; return 2; }
        s = d.x / a.y; t = -d.z / a.y; return 3;
    }
    if (d.z > 0.0) { s = d.x / a.z; t = -d.y / a.z; return 4; }
    s = -d.x / a.z; t = -d.y / a.z; return 5;
}

std::uint16_t toHalf(float value)
{
    std::uint32_t x;
    std::memcpy(&x, &value, 4);
    const std::uint32_t sign = (x >> 16) & 0x8000u;
    const int exponent = static_cast<int>((x >> 23) & 0xffu) - 127 + 15;
    std::uint32_t mantissa = x & 0x7fffffu;
    if (exponent <= 0)
        return static_cast<std::uint16_t>(sign);
    if (exponent >= 31)
        return static_cast<std::uint16_t>(sign | 0x7bffu);
    mantissa += 0x1000u; // round
    if (mantissa & 0x800000u)
    {
        mantissa = 0;
        if (exponent + 1 >= 31)
            return static_cast<std::uint16_t>(sign | 0x7bffu);
        return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent + 1) << 10));
    }
    return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10) | (mantissa >> 13));
}

bool bakeGlobeFace(const SurfaceField& field, int size, int face, GlobeFaces& out, const std::atomic<bool>* cancel)
{
    const auto start = std::chrono::steady_clock::now();
    const int n = size;
    const int b = n + 2; // one border texel for the normals
    const double radius = field.referenceRadiusM();
    // Band limit: two texels at the face centre (the face spans pi/2).
    const double band = 2.0 * radius * 1.5707963267948966 / static_cast<double>(n);
    std::vector<SurfaceSample> samples(static_cast<std::size_t>(b) * static_cast<std::size_t>(b));
    std::vector<glm::dvec3> dirs(samples.size());
    for (int y = 0; y < b; ++y)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;
        for (int x = 0; x < b; ++x)
        {
            const double s = 2.0 * (x - 1 + 0.5) / n - 1.0;
            const double t = 2.0 * (y - 1 + 0.5) / n - 1.0;
            const std::size_t i = static_cast<std::size_t>(y) * b + x;
            dirs[i] = cubeMapDirection(face, s, t);
            samples[i] = field.sample(dirs[i], band);
        }
    }
    auto& albedo = out.albedo[static_cast<std::size_t>(face)];
    auto& normal = out.normal[static_cast<std::size_t>(face)];
    auto& relief = out.relief[static_cast<std::size_t>(face)];
    albedo.assign(static_cast<std::size_t>(n) * n * 4, 0);
    normal.assign(static_cast<std::size_t>(n) * n * 4, 0);
    relief.assign(static_cast<std::size_t>(n) * n * 2, 0);
    const auto shown = [&](std::size_t i) {
        const SurfaceSample& s = samples[i];
        return s.water ? std::max(s.waterLevelM, s.heightM) : s.heightM;
    };
    for (int y = 0; y < n; ++y)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;
        for (int x = 0; x < n; ++x)
        {
            const std::size_t c = static_cast<std::size_t>(y + 1) * b + (x + 1);
            const auto p = [&](std::size_t i) { return dirs[i] * (radius + shown(i)); };
            const glm::dvec3 du = p(c + 1) - p(c - 1);
            const glm::dvec3 dv = p(c + b) - p(c - b);
            glm::dvec3 nrm = glm::normalize(glm::cross(du, dv));
            if (glm::dot(nrm, dirs[c]) < 0.0)
                nrm = -nrm;
            const double slope = 1.0 - glm::dot(nrm, dirs[c]);
            const SurfaceSample& s = samples[c];
            const SurfaceAppearance a = field.appearance(s, dirs[c], slope);
            const std::size_t o = (static_cast<std::size_t>(y) * n + x);
            const glm::vec3 enc = glm::sqrt(glm::clamp(a.albedo, glm::vec3(0.0f), glm::vec3(1.0f)));
            albedo[o * 4 + 0] = static_cast<std::uint8_t>(enc.r * 255.0f + 0.5f);
            albedo[o * 4 + 1] = static_cast<std::uint8_t>(enc.g * 255.0f + 0.5f);
            albedo[o * 4 + 2] = static_cast<std::uint8_t>(enc.b * 255.0f + 0.5f);
            albedo[o * 4 + 3] = s.water ? 255 : 0;
            normal[o * 4 + 0] = static_cast<std::uint8_t>((nrm.x * 0.5 + 0.5) * 255.0 + 0.5);
            normal[o * 4 + 1] = static_cast<std::uint8_t>((nrm.y * 0.5 + 0.5) * 255.0 + 0.5);
            normal[o * 4 + 2] = static_cast<std::uint8_t>((nrm.z * 0.5 + 0.5) * 255.0 + 0.5);
            normal[o * 4 + 3] = static_cast<std::uint8_t>(std::clamp(a.emission, 0.0f, 1.0f) * 255.0f + 0.5f);
            relief[o * 2 + 0] = toHalf(static_cast<float>(shown(c) / 1000.0));
            relief[o * 2 + 1] = toHalf(static_cast<float>(std::clamp(s.coastM, -60000.0, 60000.0) / 1000.0));
        }
    }
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    out.faceSeconds[static_cast<std::size_t>(face)] = seconds;
    return true;
}

void finishGlobeBake(GlobeFaces& out)
{
    glm::dvec3 sum(0.0);
    double count = 0.0;
    for (const auto& face : out.albedo)
        for (std::size_t i = 0; i + 3 < face.size(); i += 4 * 7)
        {
            const glm::dvec3 c(face[i] / 255.0, face[i + 1] / 255.0, face[i + 2] / 255.0);
            sum += c * c;
            count += 1.0;
        }
    if (count > 0.0)
        out.meanAlbedo = glm::vec3(sum / count);
}

void bakeCloudFace(const CloudWeatherField& clouds, std::size_t layer, double worldTimeSeconds, int size, int face,
                   CloudFaces& out)
{
    auto& cov = out.coverage[static_cast<std::size_t>(face)];
    cov.assign(static_cast<std::size_t>(size) * size, 0);
    if (layer >= clouds.layers().size())
        return;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const glm::dvec3 d = cubeMapDirection(face, 2.0 * (x + 0.5) / size - 1.0, 2.0 * (y + 0.5) / size - 1.0);
            const float c = clouds.coverage(layer, d, worldTimeSeconds);
            cov[static_cast<std::size_t>(y) * size + x] = static_cast<std::uint8_t>(std::clamp(c, 0.0f, 1.0f) * 255.0f);
        }
    out.size = size;
    out.worldTimeSeconds = worldTimeSeconds;
}
} // namespace world::celestial::surface
