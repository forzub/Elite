// Headless preview of the procedural surface of any generated body:
//   build/EliteCelestialPreview.exe <body.json> <out_prefix> [width]
// writes <out_prefix>_map.png (equirect albedo, north up, the map shaders'
// longitude convention) and <out_prefix>_globe.png (lit sphere, the view
// the system map shows). The surface is makeSurfaceField(descriptor) - the
// same field flight and the map use.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/PlanetSurfaceField.h"
#include "tools/planet_lab/PngWriter.h"

namespace surf = world::celestial::surface;

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: EliteCelestialPreview <body.json> <out_prefix> [width]\n");
        return 2;
    }
    const std::string path = argv[1], prefix = argv[2];
    const int w = argc > 3 ? std::max(64, std::atoi(argv[3])) : 1024, h = w / 2;
    surf::BodySurfaceDescriptor body;
    std::string error;
    if (!surf::loadGeneratedBody(path, body, &error))
    {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    const auto field = surf::makeSurfaceField(body);
    const double pi = 3.14159265358979323846;
    const double texelM = 2.0 * pi * field->referenceRadiusM() / w;
    const auto dirAt = [&](double x, double y) {
        const double lon = (x / w - 0.5) * 2.0 * pi;
        const double lat = (0.5 - y / h) * pi;
        return glm::dvec3(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
    };
    std::vector<glm::vec3> albedo(static_cast<std::size_t>(w) * h);
    std::vector<double> height(albedo.size());
    {
        std::vector<surf::SurfaceSample> s(albedo.size());
        const int threads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&, t] {
                for (int y = t; y < h; y += threads)
                    for (int x = 0; x < w; ++x)
                        s[static_cast<std::size_t>(y) * w + x] = field->sample(dirAt(x + 0.5, y + 0.5), texelM * 2.0);
            });
        for (auto& th : pool)
            th.join();
        for (std::size_t i = 0; i < s.size(); ++i)
            height[i] = s[i].heightM;
        pool.clear();
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&, t] {
                for (int y = t; y < h; y += threads)
                    for (int x = 0; x < w; ++x)
                    {
                        const std::size_t i = static_cast<std::size_t>(y) * w + x;
                        const double dx = height[static_cast<std::size_t>(y) * w + (x + 1) % w] - height[i];
                        const double dy = height[static_cast<std::size_t>(std::max(0, y - 1)) * w + x] - height[i];
                        const double slope = std::min(1.0, std::hypot(dx, dy) / texelM);
                        albedo[i] = field->appearance(s[i], dirAt(x + 0.5, y + 0.5),
                                                      1.0 - 1.0 / std::sqrt(1.0 + slope * slope)).albedo;
                    }
            });
        for (auto& th : pool)
            th.join();
    }
    const auto toSrgb = [](float c) {
        return static_cast<unsigned char>(std::clamp(std::pow(std::clamp(c, 0.0f, 1.0f), 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    // map (PngWriter takes GL bottom-up rows)
    {
        std::vector<unsigned char> rgb(static_cast<std::size_t>(w) * h * 3);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const glm::vec3 c = albedo[static_cast<std::size_t>(y) * w + x];
                const std::size_t o = (static_cast<std::size_t>(h - 1 - y) * w + x) * 3;
                rgb[o] = toSrgb(c.r);
                rgb[o + 1] = toSrgb(c.g);
                rgb[o + 2] = toSrgb(c.b);
            }
        planet_lab::writePng(prefix + "_map.png", w, h, rgb);
    }
    // globe: orthographic, sun from the upper left, relief shading from the height map
    {
        const int g = 512;
        double minH = 1e30, maxH = -1e30;
        for (double v : height)
        {
            minH = std::min(minH, v);
            maxH = std::max(maxH, v);
        }
        const glm::dvec3 sun = glm::normalize(glm::dvec3(-0.6, 0.5, 0.65));
        std::vector<unsigned char> rgb(static_cast<std::size_t>(g) * g * 3, 0);
        for (int py = 0; py < g; ++py)
            for (int px = 0; px < g; ++px)
            {
                const double sx = (px + 0.5) / g * 2.0 - 1.0, sy = 1.0 - (py + 0.5) / g * 2.0;
                const double rr = sx * sx + sy * sy;
                if (rr > 1.0)
                    continue;
                const glm::dvec3 n(sx, sy, std::sqrt(1.0 - rr)); // view space: +z to the viewer
                // body frame: view +z = body +x (longitude 0 faces the viewer), +y = spin axis
                const glm::dvec3 d(n.z, n.y, -n.x);
                const double lat = std::asin(std::clamp(d.y, -1.0, 1.0));
                double lon = std::atan2(d.z, d.x);
                const double u = (lon / (2.0 * pi) + 0.5) * w, v = (0.5 - lat / pi) * h;
                const int ix = std::clamp(static_cast<int>(u), 0, w - 1), iy = std::clamp(static_cast<int>(v), 0, h - 1);
                const glm::vec3 c = albedo[static_cast<std::size_t>(iy) * w + ix];
                const double lambert = std::max(0.0, glm::dot(n, sun));
                const double light = 0.04 + 0.96 * lambert;
                const std::size_t o = (static_cast<std::size_t>(g - 1 - py) * g + px) * 3;
                rgb[o] = toSrgb(c.r * static_cast<float>(light));
                rgb[o + 1] = toSrgb(c.g * static_cast<float>(light));
                rgb[o + 2] = toSrgb(c.b * static_cast<float>(light));
            }
        planet_lab::writePng(prefix + "_globe.png", g, g, rgb);
    }
    std::printf("%s: %s archetype %s, R %.0f km -> %s_map.png / _globe.png\n", body.id.c_str(), body.bodyClass.c_str(),
                surf::archetypeName(surf::surfaceArchetype(body)), field->referenceRadiusM() / 1000.0, prefix.c_str());
    return 0;
}
