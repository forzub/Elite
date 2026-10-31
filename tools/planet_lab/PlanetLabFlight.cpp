// Planet Lab flight mode: the production celestial renderer on the real
// catalog. Interactive keys:
//   Tab / Shift+Tab  next / previous target body      1..4  far / orbit / approach / surface
//   V (Ctrl+F7)      style Elite -> Modern -> Anime     N     travel to the next target
//   mouse drag       look around                        W/S   forward / back (speed ~ altitude)
//   A/D              strafe                             Space time x 600
//   P                print telemetry

#include "tools/planet_lab/PlanetLabFlight.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/render/celestial/flight/CelestialFlightRenderer.h"
#include "src/render/celestial/flight/FlightFrameBuilder.h"
#include "src/world/celestial/CelestialSystemRuntime.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/surface/LocalTerrain.h"
#include "src/world/celestial/surface/SurfaceField.h"
#include "src/game/system_map/PlanetBodyOrientation.h"
#include "tools/planet_lab/PngWriter.h"

namespace planet_lab
{
namespace
{
    namespace fs = std::filesystem;
    namespace cf = render::celestial_flight;
    namespace wc = world::celestial;
    namespace surface = world::celestial::surface;

    constexpr double Pi = 3.14159265358979323846;
    constexpr float FovY = static_cast<float>(55.0 * Pi / 180.0);

    double seconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    struct Target
    {
        int systemId = 0;
        std::string stem;   // generated body file stem
        std::string label;
    };

    const std::vector<Target>& targets()
    {
        static const std::vector<Target> t {
            {0, "planet_earth", "earth"},          {0, "moon_moon", "moon"},
            {0, "planet_mars", "mars"},            {0, "planet_jupiter", "jupiter"},
            {0, "planet_saturn", "saturn"},        {0, "planet_mercury", "mercury"},
            {41, "planet_hd_104067_g", "proc_terrestrial"}, {41, "planet_hd_104067_d", "proc_lava"},
            {41, "planet_hd_104067_e", "proc_icy"}, {41, "planet_hd_104067_b", "proc_gas_giant"},
        };
        return t;
    }

    struct Camera
    {
        glm::dvec3 pos {0.0};
        glm::dvec3 fwd {0.0, 0.0, -1.0};
        glm::dvec3 up {0.0, 1.0, 0.0};
        glm::mat4 viewRotation() const
        {
            return glm::lookAt(glm::vec3(0.0f), glm::vec3(fwd), glm::vec3(up));
        }
    };

    class Lab
    {
    public:
        explicit Lab(std::string root) : m_root(std::move(root)), m_builder(m_root) {}

        bool init(std::string& error)
        {
            if (!m_atlas.load(m_root + "/src/assets/data/galaxy_details"))
            {
                error = "cannot load the catalog under " + m_root + "/src/assets/data/galaxy_details";
                return false;
            }
            return true;
        }

        bool selectSystem(int id, double time)
        {
            const wc::CelestialSystemDefinition* def = m_atlas.findSystem(id);
            if (!def)
                return false;
            if (def != m_system)
            {
                m_system = def;
                m_runtime.setSystem(def);
            }
            m_time = time;
            m_runtime.update(time);
            return true;
        }

        const wc::CelestialBodyState* body(const std::string& stem) const
        {
            for (const auto& b : m_runtime.snapshot().bodies)
                if (fs::path(b.sourcePath).stem().string() == stem)
                    return &b;
            return nullptr;
        }

        void setTime(double t)
        {
            m_time = t;
            m_runtime.update(t);
        }

        double time() const { return m_time; }
        const wc::CelestialSystemSnapshot& snapshot() const { return m_runtime.snapshot(); }

        void frame(cf::CelestialFlightRenderer& r, const Camera& cam, int w, int h)
        {
            m_builder.build(*m_system, m_runtime.snapshot(), cam.pos, m_time, m_frame);
            glViewport(0, 0, w, h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            r.render(m_frame, cam.viewRotation(), FovY, w, h);
        }

        // Body-fixed axes of a body state (the maps' orientation function).
        static glm::dmat3 axes(const wc::CelestialBodyState& b)
        {
            const auto o = game::system_map::makePlanetTextureOrientation(b.axialTiltDeg, b.axisNodeDeg,
                                                                          b.rotationPhaseRad,
                                                                          b.textureLongitudeOffsetDeg);
            return glm::dmat3(o.prime, o.north, o.east);
        }

        glm::dvec3 sunDirection(const wc::CelestialBodyState& b) const
        {
            for (const auto& s : m_runtime.snapshot().bodies)
                if (s.type == wc::BodyType::Star)
                    return glm::normalize(s.worldMeters - b.worldMeters);
            return glm::dvec3(1.0, 0.0, 0.0);
        }

    private:
        std::string m_root;
        wc::StarAtlasDatabase m_atlas;
        const wc::CelestialSystemDefinition* m_system = nullptr;
        wc::CelestialSystemRuntime m_runtime;
        cf::FlightFrameBuilder m_builder;
        cf::FlightCelestialFrame m_frame;
        double m_time = 0.0;
    };

    // Renders until the renderer has nothing pending (3 quiet frames).
    double converge(Lab& lab, cf::CelestialFlightRenderer& r, const Camera& cam, GLFWwindow* win, int w, int h,
                    double timeout)
    {
        const double start = seconds();
        int quiet = 0;
        while (seconds() - start < timeout)
        {
            lab.frame(r, cam, w, h);
            glfwSwapBuffers(win);
            glfwPollEvents();
            if (r.stats().pendingJobs == 0)
            {
                if (++quiet >= 3)
                    break;
            }
            else
            {
                quiet = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        return seconds() - start;
    }

    // A day-side site with oblique light and character (coast / relief).
    glm::dvec3 chooseSite(const surface::SurfaceField* field, const glm::dvec3& sunBody)
    {
        glm::dvec3 best = glm::normalize(sunBody);
        if (!field)
            return best;
        double bestScore = -1.0e30;
        const double R = field->referenceRadiusM();
        const int n = 3000;
        // Oblique light (sun 18..36 deg above the horizon) reads relief best;
        // when that band is all ocean, the whole day side is searched.
        for (int pass = 0; pass < 2 && bestScore < -5.0e5; ++pass)
        for (int i = 0; i < n; ++i)
        {
            const double y = 1.0 - 2.0 * (i + 0.5) / n;
            const double rr = std::sqrt(std::max(0.0, 1.0 - y * y));
            const double a = i * 2.399963229728653;
            const glm::dvec3 d(rr * std::cos(a), y, rr * std::sin(a));
            const double sunAngle = std::acos(std::clamp(glm::dot(d, glm::normalize(sunBody)), -1.0, 1.0));
            const double lo0 = pass == 0 ? 0.95 : 0.35, hi0 = pass == 0 ? 1.25 : 1.35;
            if (sunAngle < lo0 || sunAngle > hi0 || std::abs(y) > 0.65)
                continue;
            const surface::SurfaceSample s = field->sample(d, 5000.0);
            double score = 0.0;
            glm::dvec3 t = glm::normalize(glm::cross(d, glm::dvec3(0.0, 1.0, 0.0)));
            const glm::dvec3 b = glm::cross(d, t);
            double lo = 1.0e30, hi = -1.0e30;
            for (int k = 0; k < 4; ++k)
            {
                const glm::dvec3 q = glm::normalize(d + (k & 1 ? t : b) * ((k & 2 ? 1.0 : -1.0) * 30000.0 / R));
                const surface::SurfaceSample sq = field->sample(q, 5000.0);
                lo = std::min(lo, sq.heightM);
                hi = std::max(hi, sq.heightM);
            }
            score = hi - lo;
            if (field->solidSurface())
            {
                if (s.water)
                    score -= 1.0e6; // never water when land exists
                if (s.coastM < 1500.0 && !s.water)
                    score += 2000.0; // coastline in view
                if (s.iceMask > 0.5 || s.temperatureK < 255.0f)
                    score -= 3000.0; // not on an ice sheet
            }
            if (score > bestScore)
            {
                bestScore = score;
                best = d;
            }
        }
        return best;
    }

    enum View
    {
        FarSpace,
        NearOrbit,
        Approach,
        NearSurface,
        ViewCount
    };
    const char* viewName(int v)
    {
        static const char* n[] = {"far_space", "near_orbit", "approach", "near_surface"};
        return n[v];
    }

    Camera viewpoint(Lab& lab, const wc::CelestialBodyState& b, const surface::SurfaceField* field, int view,
                     glm::dvec3& siteBodyOut)
    {
        const double R = b.radiusKm * 1000.0;
        const glm::dmat3 ax = Lab::axes(b);
        const glm::dvec3 sunWorld = lab.sunDirection(b);
        const glm::dvec3 sunBody = glm::transpose(ax) * sunWorld;
        if (glm::length(siteBodyOut) < 0.5)
            siteBodyOut = chooseSite(field, sunBody);
        const glm::dvec3 n = ax * siteBodyOut;
        const glm::dvec3 side = glm::normalize(glm::cross(n, sunWorld));
        Camera c;
        if (view == FarSpace)
        {
            c.pos = b.worldMeters + glm::normalize(n * 0.8 + side * 0.6) * R * 30.0;
            c.fwd = glm::normalize(b.worldMeters - c.pos);
            c.up = glm::normalize(glm::cross(glm::cross(c.fwd, glm::dvec3(0, 1, 0)), c.fwd));
            return c;
        }
        double ground = 0.0;
        if (field)
            ground = std::max(0.0, field->sample(siteBodyOut, 1000.0).heightM);
        const bool solid = field && field->solidSurface();
        double alt = view == NearOrbit ? 0.06 * R : view == Approach ? 0.004 * R : std::max(220.0, 3.0e-5 * R);
        if (!solid && view == NearSurface)
            alt = 0.0012 * R;
        const double pitch = (view == NearOrbit ? 24.0 : view == Approach ? 13.0 : 5.0) * Pi / 180.0;
        c.pos = b.worldMeters + n * (R + ground + alt);
        c.fwd = glm::normalize(side * std::cos(pitch) - n * std::sin(pitch));
        c.up = glm::normalize(n - c.fwd * glm::dot(n, c.fwd));
        return c;
    }

    void readPixels(int w, int h, std::vector<unsigned char>& rgb)
    {
        rgb.resize(static_cast<std::size_t>(w) * h * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    }

    GLFWwindow* openWindow(int w, int h, bool visible)
    {
        if (!glfwInit())
            return nullptr;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
        GLFWwindow* win = glfwCreateWindow(w, h, "Planet Lab - flight (production renderer)", nullptr, nullptr);
        if (!win)
            return nullptr;
        glfwMakeContextCurrent(win);
        gladLoadGL(glfwGetProcAddress);
        glfwSwapInterval(0);
        return win;
    }

    std::string telemetry(const cf::CelestialFlightRenderer& r)
    {
        const auto& s = r.stats();
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "%s | alt %.0f m (%s) | cpu %.2f+%.2f ms (upload %.1f) | gpu globe %.2f terr %.2f cloud %.2f atm %.2f ring %.2f "
                      "spr %.2f ast %.2f | bodies %d+%d spr | terrain %d lv %lld tri | tex %.0f MB mesh %.1f MB | jobs %d",
                      cf::celestialRenderStyleId(r.style()), s.nearestAltitudeM, s.nearestBody.c_str(), s.cpuPrepareMs,
                      s.cpuDrawMs, s.cpuUploadMs, s.gpuGlobesMs, s.gpuTerrainMs, s.gpuCloudsMs, s.gpuAtmosphereMs, s.gpuRingsMs,
                      s.gpuStarsSpritesMs, s.gpuAsteroidsMs, s.globeBodies, s.spriteBodies, s.terrainLevels,
                      s.terrainTriangles, s.textureBytes / 1048576.0, s.meshBytes / 1048576.0, s.pendingJobs);
        return buf;
    }

    // ---- capture --------------------------------------------------------
    int capture(Lab& lab, cf::CelestialFlightRenderer& r, GLFWwindow* win, int w, int h, const std::string& dir,
                double t0)
    {
        fs::create_directories(dir);
        std::ofstream log(dir + "/flight_capture.txt");
        // PLANET_LAB_FLAGS=noatm,noclouds,norings,nodetail (diagnostics).
        if (const char* flags = std::getenv("PLANET_LAB_FLAGS"))
        {
            const std::string f = flags;
            cf::FlightRenderQuality q = r.quality();
            q.atmosphere = f.find("noatm") == std::string::npos;
            q.clouds = f.find("noclouds") == std::string::npos;
            q.rings = f.find("norings") == std::string::npos;
            q.detailNormals = f.find("nodetail") == std::string::npos;
            r.setQuality(q);
        }
        const cf::CelestialRenderStyle styles[] = {cf::CelestialRenderStyle::EliteStyle,
                                                   cf::CelestialRenderStyle::Modern, cf::CelestialRenderStyle::Anime};
        // PLANET_LAB_ONLY=earth,moon limits the targets ("travel" = the
        // Earth -> Moon sequence).
        const char* onlyEnv = std::getenv("PLANET_LAB_ONLY");
        const std::string only = onlyEnv ? onlyEnv : "";
        const auto wanted = [&](const std::string& label) {
            return only.empty() || only.find(label) != std::string::npos;
        };
        for (const Target& t : targets())
        {
            if (!wanted(t.label) || !lab.selectSystem(t.systemId, t0))
                continue;
            const wc::CelestialBodyState* b = lab.body(t.stem);
            if (!b)
            {
                std::printf("missing target %s\n", t.stem.c_str());
                continue;
            }
            // Load the field first (far view), then choose the site.
            glm::dvec3 site(0.0);
            Camera far = viewpoint(lab, *b, nullptr, FarSpace, site);
            r.setStyle(cf::CelestialRenderStyle::Modern);
            converge(lab, r, far, win, w, h, 30.0);
            const auto field = r.surfaceField(b->id);
            site = glm::dvec3(0.0);
            for (int v = 0; v < ViewCount; ++v)
            {
                const Camera cam = viewpoint(lab, *b, field.get(), v, site);
                if (v == 0 && field)
                {
                    const surface::SurfaceSample ss = field->sample(site, 5000.0);
                    std::printf("site %s: lat %.1f lon %.1f water %d coast %.0f m height %.0f m\n", t.label.c_str(),
                                std::asin(site.y) * 180.0 / Pi, std::atan2(site.z, site.x) * 180.0 / Pi,
                                ss.water ? 1 : 0, ss.coastM, ss.heightM);
                }
                for (const auto style : styles)
                {
                    r.setStyle(style);
                    const double secs = converge(lab, r, cam, win, w, h, 90.0);
                    lab.frame(r, cam, w, h);
                    glFinish();
                    std::vector<unsigned char> rgb;
                    readPixels(w, h, rgb);
                    const std::string name = "flight_" + t.label + "_" + std::to_string(v + 1) + "_" + viewName(v) +
                                             "_" + cf::celestialRenderStyleId(style) + ".png";
                    writePng(dir + "/" + name, w, h, rgb);
                    const double sunElevation =
                        std::asin(std::clamp(glm::dot(glm::normalize(cam.pos - b->worldMeters), lab.sunDirection(*b)),
                                             -1.0, 1.0)) * 180.0 / Pi;
                    const std::string line = name + " | sun elevation " + std::to_string(int(sunElevation)) +
                                             " deg | converged " + std::to_string(secs) + " s | " + telemetry(r);
                    std::printf("%s\n", line.c_str());
                    log << line << "\n";
                    glfwSwapBuffers(win);
                    glfwPollEvents();
                }
            }
        }
        // Travel Earth -> Moon: the same world at shrinking / growing sizes.
        if (wanted("travel") && lab.selectSystem(0, t0))
        {
            const wc::CelestialBodyState* earth = lab.body("planet_earth");
            const wc::CelestialBodyState* moon = lab.body("moon_moon");
            if (earth && moon)
            {
                r.setStyle(cf::CelestialRenderStyle::Modern);
                const glm::dvec3 a = earth->worldMeters;
                const glm::dvec3 bpos = moon->worldMeters;
                const glm::dvec3 axis = glm::normalize(bpos - a);
                const double Re = earth->radiusKm * 1000.0, Rm = moon->radiusKm * 1000.0;
                const glm::dvec3 off = glm::normalize(glm::cross(axis, glm::dvec3(0, 1, 0)));
                const double total = glm::length(bpos - a);
                for (int k = 0; k <= 7; ++k)
                {
                    const double f = k / 7.0;
                    // Leaving Earth from 2000 km altitude to 3 Moon radii from the Moon.
                    const double s = Re + 2.0e6 + (total - Re - 2.0e6 - 3.0 * Rm) * (1.0 - std::cos(f * Pi)) * 0.5;
                    Camera c;
                    c.pos = a + axis * s + off * (Re * 0.6 * (1.0 - f) + Rm * 1.2 * f);
                    // Look back at Earth at the start, ahead at the Moon later.
                    c.fwd = f < 0.4 ? glm::normalize(a - c.pos) : glm::normalize(bpos - c.pos);
                    c.up = glm::normalize(glm::cross(glm::cross(c.fwd, glm::dvec3(0, 1, 0)), c.fwd));
                    converge(lab, r, c, win, w, h, 30.0);
                    lab.frame(r, c, w, h);
                    glFinish();
                    std::vector<unsigned char> rgb;
                    readPixels(w, h, rgb);
                    const std::string name = "flight_travel_earth_moon_" + std::to_string(k) + ".png";
                    writePng(dir + "/" + name, w, h, rgb);
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "%s | from Earth %.0f km, to Moon %.0f km | ", name.c_str(),
                                  glm::length(c.pos - a) / 1000.0, glm::length(c.pos - bpos) / 1000.0);
                    log << buf << telemetry(r) << "\n";
                    std::printf("%s%s\n", buf, telemetry(r).c_str());
                }
            }
        }
        return 0;
    }

    // ---- benchmark ------------------------------------------------------
    int bench(Lab& lab, cf::CelestialFlightRenderer& r, GLFWwindow* win, int w, int h, const std::string& dir,
              double t0)
    {
        fs::create_directories(dir);
        std::ofstream out(dir + "/flight_bench.txt");
        const auto print = [&](const std::string& s) {
            std::printf("%s\n", s.c_str());
            out << s << "\n";
        };
        char hdr[512];
        std::snprintf(hdr, sizeof(hdr), "viewport %dx%d, GL %s | %s", w, h,
                      reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                      reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        print(hdr);
        print("target view style | cold_s | frame_ms cpu_ms gpu_total_ms | globe terr cloud atm ring spr ast (gpu ms) "
              "| tris | tex_MB mesh_MB");
        GLuint q = 0;
        glGenQueries(1, &q);
        const cf::CelestialRenderStyle styles[] = {cf::CelestialRenderStyle::EliteStyle,
                                                   cf::CelestialRenderStyle::Modern, cf::CelestialRenderStyle::Anime};
        const Target benchTargets[] = {{0, "planet_earth", "earth"}, {0, "planet_saturn", "saturn"},
                                       {41, "planet_hd_104067_g", "proc_terrestrial"}};
        for (const Target& t : benchTargets)
        {
            lab.selectSystem(t.systemId, t0);
            const wc::CelestialBodyState* b = lab.body(t.stem);
            if (!b)
                continue;
            glm::dvec3 site(0.0);
            converge(lab, r, viewpoint(lab, *b, nullptr, FarSpace, site), win, w, h, 30.0);
            const auto field = r.surfaceField(b->id);
            site = glm::dvec3(0.0);
            for (int v = 0; v < ViewCount; ++v)
            {
                const Camera cam = viewpoint(lab, *b, field.get(), v, site);
                if (v == 0 && field)
                {
                    const surface::SurfaceSample ss = field->sample(site, 5000.0);
                    std::printf("site %s: lat %.1f lon %.1f water %d coast %.0f m height %.0f m\n", t.label.c_str(),
                                std::asin(site.y) * 180.0 / Pi, std::atan2(site.z, site.x) * 180.0 / Pi,
                                ss.water ? 1 : 0, ss.coastM, ss.heightM);
                }
                for (const auto style : styles)
                {
                    r.setStyle(style);
                    const double cold = converge(lab, r, cam, win, w, h, 90.0);
                    const int frames = 90;
                    double cpu = 0.0, wall = 0.0, gpuTotal = 0.0, worst = 0.0;
                    std::array<double, 7> cat {};
                    long long tris = 0;
                    for (int i = 0; i < frames; ++i)
                    {
                        const double f0 = seconds();
                        glBeginQuery(GL_TIME_ELAPSED, q);
                        lab.frame(r, cam, w, h);
                        glEndQuery(GL_TIME_ELAPSED);
                        glFinish();
                        wall += seconds() - f0;
                        worst = std::max(worst, seconds() - f0);
                        GLuint64 ns = 0;
                        glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
                        gpuTotal += ns * 1.0e-6;
                        const auto& s = r.stats();
                        cpu += s.cpuPrepareMs + s.cpuDrawMs;
                        const double c[7] = {s.gpuGlobesMs, s.gpuTerrainMs, s.gpuCloudsMs, s.gpuAtmosphereMs,
                                             s.gpuRingsMs, s.gpuStarsSpritesMs, s.gpuAsteroidsMs};
                        for (int k = 0; k < 7; ++k)
                            cat[static_cast<std::size_t>(k)] += c[k];
                        tris = s.terrainTriangles;
                        glfwSwapBuffers(win);
                        glfwPollEvents();
                    }
                    const auto& s = r.stats();
                    char line[512];
                    std::snprintf(line, sizeof(line),
                                  "%s %s %s | %.2f | %.2f %.2f %.2f max %.1f | %.2f %.2f %.2f %.2f %.2f %.2f %.2f | %lld | %.0f %.1f",
                                  t.label.c_str(), viewName(v), cf::celestialRenderStyleId(style), cold,
                                  wall * 1000.0 / frames, cpu / frames, gpuTotal / frames, worst * 1000.0, cat[0] / frames,
                                  cat[1] / frames, cat[2] / frames, cat[3] / frames, cat[4] / frames, cat[5] / frames,
                                  cat[6] / frames, tris, s.textureBytes / 1048576.0, s.meshBytes / 1048576.0);
                    print(line);
                }
            }
        }
        // Streaming: samples per second of the worker pool.
        const auto& s = r.stats();
        char tail[256];
        std::snprintf(tail, sizeof(tail), "workers busy %.1f s total, %lld field samples (%.0f samples / busy s)",
                      s.workerBusySeconds, s.samplesTaken,
                      s.workerBusySeconds > 0 ? s.samplesTaken / s.workerBusySeconds : 0.0);
        print(tail);
        glDeleteQueries(1, &q);
        return 0;
    }

    // ---- checks ---------------------------------------------------------
    int checks(Lab& lab, cf::CelestialFlightRenderer& r, GLFWwindow* win, int w, int h, double t0)
    {
        int failed = 0;
        const auto report = [&](bool ok, const std::string& what) {
            std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
            failed += ok ? 0 : 1;
        };
        lab.selectSystem(0, t0);
        // 1. Angular size and position on screen (Elite rim line = silhouette;
        //    the atmosphere outline is switched off so only the body counts).
        r.setStyle(cf::CelestialRenderStyle::EliteStyle);
        cf::FlightRenderQuality q = r.quality();
        q.atmosphere = false;
        q.clouds = false;
        q.rings = false;
        r.setQuality(q);
        for (const char* stem : {"moon_moon", "planet_mars", "planet_jupiter"})
        {
            const wc::CelestialBodyState* b = lab.body(stem);
            if (!b)
            {
                report(false, std::string("body ") + stem);
                continue;
            }
            const double R = b->radiusKm * 1000.0;
            for (double k : {4.0, 15.0, 60.0})
                for (double offDeg : {0.0, 12.0})
                {
                    const glm::dvec3 sun = lab.sunDirection(*b);
                    Camera c;
                    const glm::dvec3 toBody = glm::normalize(glm::cross(sun, glm::dvec3(0, 1, 0)));
                    c.pos = b->worldMeters - toBody * (R * k);
                    const double off = offDeg * Pi / 180.0;
                    const glm::dvec3 right = glm::normalize(glm::cross(toBody, glm::dvec3(0, 1, 0)));
                    c.fwd = glm::normalize(toBody * std::cos(off) - right * std::sin(off));
                    c.up = glm::normalize(glm::cross(right, c.fwd));
                    converge(lab, r, c, win, w, h, 30.0);
                    lab.frame(r, c, w, h);
                    glFinish();
                    std::vector<unsigned char> rgb;
                    readPixels(w, h, rgb);
                    // Expected silhouette: angular radius asin(R/d); projected
                    // centre at tan(off) in the image plane.
                    const double d = R * k;
                    const double alpha = std::asin(R / d);
                    const double f = (h * 0.5) / std::tan(FovY * 0.5);
                    const double cx = w * 0.5 + f * std::tan(off);
                    const double left = w * 0.5 + f * std::tan(off - alpha);
                    const double rightE = w * 0.5 + f * std::tan(off + alpha);
                    const int row = h / 2;
                    int lo = -1, hi = -1;
                    // Only the neighbourhood of the expected disc: other
                    // bodies (Jupiter's moons) may share the row.
                    const int x0 = std::max(0, static_cast<int>(left) - 8);
                    const int x1 = std::min(w, static_cast<int>(rightE) + 8);
                    // Seven rows around the centre row: a 1-px outline can
                    // fall between pixel centres on a single row.
                    for (int dy = -3; dy <= 3; ++dy)
                        for (int x = x0; x < x1; ++x)
                        {
                            const unsigned char* p = &rgb[(static_cast<std::size_t>(row + dy) * w + x) * 3];
                            if (p[1] > 40)
                            {
                                if (lo < 0 || x < lo)
                                    lo = x;
                                hi = std::max(hi, x);
                            }
                        }
                    const double measuredLeft = lo, measuredRight = hi + 1;
                    const bool ok = lo >= 0 && std::abs(measuredLeft - left) < 2.5 &&
                                    std::abs(measuredRight - rightE) < 2.5;
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                                  "%s at %.0f R, %.0f deg off-axis: silhouette %.1f..%.1f px (expected %.1f..%.1f, "
                                  "centre %.1f)",
                                  stem, k, offDeg, measuredLeft, measuredRight, left, rightE, cx);
                    report(ok, buf);
                    if (!ok)
                    {
                        fs::create_directories("D:/tmp/elite_flight_check");
                        writePng("D:/tmp/elite_flight_check/" + std::string(stem) + "_" + std::to_string(int(k)) + "_" +
                                     std::to_string(int(offDeg)) + ".png",
                                 w, h, rgb);
                    }
                }
        }
        q.atmosphere = true;
        q.clouds = true;
        q.rings = true;
        r.setQuality(q);
        // 2. Ephemeris: positions follow the velocities, spins turn the right way.
        {
            const wc::CelestialBodyState* e0 = lab.body("planet_earth");
            if (e0)
            {
                const wc::CelestialBodyState earth = *e0;
                lab.setTime(t0 + 60.0);
                const wc::CelestialBodyState* e1 = lab.body("planet_earth");
                const double err = glm::length((e1->worldMeters - earth.worldMeters) -
                                               earth.worldVelocityMetersPerSecond * 60.0);
                char buf[256];
                std::snprintf(buf, sizeof(buf), "Earth moves with its velocity: 60 s drift error %.1f m of %.0f m", err,
                              glm::length(earth.worldVelocityMetersPerSecond) * 60.0);
                report(err < 50.0, buf);
                lab.setTime(t0 + 3600.0);
                const wc::CelestialBodyState* e2 = lab.body("planet_earth");
                const glm::dmat3 a0 = Lab::axes(earth), a1 = Lab::axes(*e2);
                const double turn = std::atan2(glm::dot(a1[0], a0[2]), glm::dot(a1[0], a0[0])) * 180.0 / Pi;
                std::snprintf(buf, sizeof(buf), "Earth spin in 1 h: prime meridian turned %.2f deg towards east "
                              "(expected %.2f)", turn, 360.0 / 23.9345);
                report(std::abs(turn - 360.0 / 23.9345) < 0.2, buf);
                lab.setTime(t0);
            }
        }
        // 3. Local terrain: lattice vertices are a function of the lattice point.
        {
            const wc::CelestialBodyState* b = lab.body("planet_earth");
            glm::dvec3 site(0.0);
            converge(lab, r, viewpoint(lab, *b, nullptr, FarSpace, site), win, w, h, 30.0);
            const auto field = r.surfaceField(b->id);
            if (field)
            {
                site = chooseSite(field.get(), glm::transpose(Lab::axes(*b)) * lab.sunDirection(*b));
                const int face = surface::terrain_chart::dominantFace(site);
                double u = 0.0, v = 0.0;
                surface::terrain_chart::coordinates(face, site, u, v);
                const int level = 12;
                const double du = surface::terrain_chart::step(level);
                const std::int64_t ci = static_cast<std::int64_t>(std::llround(u / du * 0.5)) * 2;
                const std::int64_t cj = static_cast<std::int64_t>(std::llround(v / du * 0.5)) * 2;
                surface::TerrainLevel a, c, coarse;
                surface::buildTerrainLevel(*field, {face, level, ci, cj}, 64, a);
                surface::buildTerrainLevel(*field, {face, level, ci + 16, cj - 8}, 64, c);
                surface::buildTerrainLevel(*field, {face, level + 1, ci / 2, cj / 2}, 64, coarse);
                int shared = 0, differ = 0, coastShared = 0, coastDiffer = 0;
                for (int j = 0; j <= 64; ++j)
                    for (int i = 0; i <= 64; ++i)
                    {
                        const int i2 = i - 16, j2 = j + 8;
                        if (i2 >= 0 && i2 <= 64 && j2 >= 0 && j2 <= 64)
                        {
                            const auto& va = a.vertices[static_cast<std::size_t>(j) * 65 + i];
                            const auto& vc = c.vertices[static_cast<std::size_t>(j2) * 65 + i2];
                            ++shared;
                            differ += (va.heightM != vc.heightM || va.coastM != vc.coastM ||
                                       std::memcmp(va.land, vc.land, 4) != 0)
                                ? 1 : 0;
                        }
                        // Even lattice points exist on the coarse level too:
                        // the coast value is LOD-independent.
                        const std::int64_t li = ci - 32 + i, lj = cj - 32 + j;
                        if ((li & 1) == 0 && (lj & 1) == 0)
                        {
                            const std::int64_t ki = li / 2 - (ci / 2 - 32), kj = lj / 2 - (cj / 2 - 32);
                            if (ki >= 0 && ki <= 64 && kj >= 0 && kj <= 64)
                            {
                                ++coastShared;
                                const auto& vk = coarse.vertices[static_cast<std::size_t>(kj) * 65 + ki];
                                coastDiffer += (vk.coastM < 0.0f) != (a.vertices[static_cast<std::size_t>(j) * 65 + i]
                                                                          .coastM < 0.0f)
                                    ? 1 : 0;
                            }
                        }
                    }
                char buf[256];
                std::snprintf(buf, sizeof(buf), "terrain level rebuilt at another centre: %d shared lattice vertices, "
                              "%d differ", shared, differ);
                report(shared > 1000 && differ == 0, buf);
                std::snprintf(buf, sizeof(buf), "terrain level k vs k+1: %d shared lattice points, %d water/land "
                              "differences", coastShared, coastDiffer);
                report(coastShared > 500 && coastDiffer == 0, buf);
            }
            else
                report(false, "Earth surface field loaded");
        }
        std::printf("flight checks: %d failed\n", failed);
        return failed == 0 ? 0 : 1;
    }

    // ---- interactive ----------------------------------------------------
    struct Interactive
    {
        Lab* lab = nullptr;
        cf::CelestialFlightRenderer* r = nullptr;
        Camera cam;
        int target = 0;
        int view = FarSpace;
        bool travel = false;
        glm::dvec3 travelFrom {0.0};
        Camera travelTo;
        double travelT = 0.0;
        double lastX = 0.0, lastY = 0.0;
        bool drag = false;
        double timeScale = 1.0;
    };

    Interactive* g_ui = nullptr;

    void jump(Interactive& ui, bool animate)
    {
        const Target& t = targets()[static_cast<std::size_t>(ui.target)];
        ui.lab->selectSystem(t.systemId, ui.lab->time());
        const wc::CelestialBodyState* b = ui.lab->body(t.stem);
        if (!b)
            return;
        glm::dvec3 site(0.0);
        const Camera c = viewpoint(*ui.lab, *b, ui.r->surfaceField(b->id).get(), ui.view, site);
        if (animate)
        {
            ui.travel = true;
            ui.travelFrom = ui.cam.pos;
            ui.travelTo = c;
            ui.travelT = 0.0;
        }
        else
            ui.cam = c;
    }
} // namespace

int runFlight(const FlightOptions& o)
{
    const bool interactive = o.captureDir.empty() && o.benchDir.empty() && !o.check;
    const int w = 1600, h = 900;
    GLFWwindow* win = openWindow(w, h, true);
    if (!win)
    {
        std::fprintf(stderr, "Planet Lab flight: no GL window\n");
        return 2;
    }
    int result = 0;
    {
        Lab lab(o.root);
        std::string error;
        if (!lab.init(error))
        {
            std::fprintf(stderr, "Planet Lab flight: %s\n", error.c_str());
            glfwTerminate();
            return 2;
        }
        cf::CelestialFlightRenderer renderer;
        if (!renderer.initialize())
        {
            std::fprintf(stderr, "Planet Lab flight: renderer initialisation failed\n");
            glfwTerminate();
            return 2;
        }
        // Real data: assets/ are under the root.
        std::error_code ec;
        fs::current_path(o.root, ec);
        lab.selectSystem(0, o.universeTimeSeconds);
        if (!o.captureDir.empty())
            result = capture(lab, renderer, win, w, h, o.captureDir, o.universeTimeSeconds);
        else if (!o.benchDir.empty())
            result = bench(lab, renderer, win, w, h, o.benchDir, o.universeTimeSeconds);
        else if (o.check)
            result = checks(lab, renderer, win, w, h, o.universeTimeSeconds);
        if (interactive)
        {
            Interactive ui;
            ui.lab = &lab;
            ui.r = &renderer;
            g_ui = &ui;
            jump(ui, false);
            glfwSetKeyCallback(win, [](GLFWwindow*, int key, int, int action, int mods) {
                if (action != GLFW_PRESS || !g_ui)
                    return;
                Interactive& ui = *g_ui;
                const int n = static_cast<int>(targets().size());
                if (key == GLFW_KEY_TAB)
                {
                    ui.target = (ui.target + ((mods & GLFW_MOD_SHIFT) ? n - 1 : 1)) % n;
                    jump(ui, false);
                }
                else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4)
                {
                    ui.view = key - GLFW_KEY_1;
                    jump(ui, false);
                }
                else if (key == GLFW_KEY_V || (key == GLFW_KEY_F7 && (mods & GLFW_MOD_CONTROL)))
                    ui.r->setStyle(cf::nextCelestialRenderStyle(ui.r->style()));
                else if (key == GLFW_KEY_N)
                {
                    ui.target = (ui.target + 1) % n;
                    if (targets()[static_cast<std::size_t>(ui.target)].systemId != ui.lab->snapshot().systemId)
                        ui.target = 0;
                    ui.view = NearOrbit;
                    jump(ui, true);
                }
                else if (key == GLFW_KEY_SPACE)
                    ui.timeScale = ui.timeScale > 1.0 ? 1.0 : 600.0;
                else if (key == GLFW_KEY_P)
                    std::printf("%s\n", telemetry(*ui.r).c_str());
            });
            double last = seconds();
            double titleAt = 0.0;
            while (!glfwWindowShouldClose(win))
            {
                glfwPollEvents();
                const double now = seconds();
                const double dt = std::min(0.1, now - last);
                last = now;
                lab.setTime(lab.time() + dt * ui.timeScale);
                // Mouse look.
                double mx, my;
                glfwGetCursorPos(win, &mx, &my);
                const bool down = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                if (down && ui.drag)
                {
                    const double yaw = -(mx - ui.lastX) * 0.003, pitch = -(my - ui.lastY) * 0.003;
                    const glm::dvec3 right = glm::normalize(glm::cross(ui.cam.fwd, ui.cam.up));
                    ui.cam.fwd = glm::normalize(glm::dvec3(glm::rotate(glm::dmat4(1.0), yaw, ui.cam.up) *
                                                           glm::dvec4(ui.cam.fwd, 0.0)));
                    ui.cam.fwd = glm::normalize(glm::dvec3(glm::rotate(glm::dmat4(1.0), pitch, right) *
                                                           glm::dvec4(ui.cam.fwd, 0.0)));
                    ui.cam.up = glm::normalize(glm::cross(right, ui.cam.fwd));
                }
                ui.drag = down;
                ui.lastX = mx;
                ui.lastY = my;
                const double alt = std::max(50.0, renderer.stats().nearestAltitudeM);
                const double speed = alt * 0.6;
                const glm::dvec3 right = glm::normalize(glm::cross(ui.cam.fwd, ui.cam.up));
                if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) ui.cam.pos += ui.cam.fwd * speed * dt;
                if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) ui.cam.pos -= ui.cam.fwd * speed * dt;
                if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) ui.cam.pos += right * speed * dt;
                if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) ui.cam.pos -= right * speed * dt;
                if (ui.travel)
                {
                    // Log-distance travel: fast in empty space, slow near bodies.
                    ui.travelT = std::min(1.0, ui.travelT + dt / 8.0);
                    const double s = (1.0 - std::cos(ui.travelT * Pi)) * 0.5;
                    ui.cam.pos = glm::mix(ui.travelFrom, ui.travelTo.pos, s);
                    const glm::dvec3 toTarget = ui.travelTo.pos - ui.cam.pos;
                    if (glm::length(toTarget) > 1.0)
                        ui.cam.fwd = glm::normalize(glm::mix(glm::normalize(toTarget), ui.travelTo.fwd, s * s));
                    ui.cam.up = glm::normalize(glm::mix(ui.cam.up, ui.travelTo.up, s));
                    if (ui.travelT >= 1.0)
                    {
                        ui.travel = false;
                        ui.cam = ui.travelTo;
                    }
                }
                int fw = w, fh = h;
                glfwGetFramebufferSize(win, &fw, &fh);
                lab.frame(renderer, ui.cam, fw, fh);
                glfwSwapBuffers(win);
                if (now > titleAt)
                {
                    titleAt = now + 0.5;
                    const std::string title = targets()[static_cast<std::size_t>(ui.target)].label + " | " +
                                              telemetry(renderer);
                    glfwSetWindowTitle(win, title.c_str());
                }
            }
            g_ui = nullptr;
        }
        renderer.shutdown();
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return result;
}
} // namespace planet_lab
