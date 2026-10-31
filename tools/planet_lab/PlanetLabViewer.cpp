// Planet Lab viewer: cube-sphere quadtree LOD over the production surface
// field, streamed on worker threads, drawn camera-relative with a
// logarithmic depth buffer (ground metres on a 6000-km planet).
//
// Keys:   1..6        body (earth_like, rocky, asteroid, gas_giant, dry, hostile)
//         F1..F5      viewpoint: far orbit, low orbit, regional, approach, ground
//         mouse L     turn the view (heading / pitch)
//         mouse R     move the focus point over the surface
//         wheel       altitude
//         arrows      move the focus point
//         L           LOD cap: none / 3 / 6 / 9 (geography must not change)
//         C           clouds on / off      W  wireframe
//         T           weather +6 h         Space  weather time running
//         P           print telemetry      Esc  quit

#include "tools/planet_lab/PlanetLabViewer.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "src/world/celestial/surface/CubeSphere.h"
#include "src/world/celestial/surface/SurfacePatch.h"

namespace planet_lab
{
namespace
{
    constexpr int Resolution = 32;
    constexpr double Pi = 3.14159265358979323846;
    constexpr double FovY = 50.0 * Pi / 180.0;
    constexpr double FarPlane = 1.0e11;

    // ---- PNG (stored deflate) ---------------------------------------------
    std::uint32_t crc32(const unsigned char* p, std::size_t n, std::uint32_t c = 0xFFFFFFFFu)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            c ^= p[i];
            for (int k = 0; k < 8; ++k)
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
        return c;
    }

    void writePng(const std::string& path, int w, int h, const std::vector<unsigned char>& rgb)
    {
        std::vector<unsigned char> raw;
        raw.reserve(static_cast<std::size_t>((w * 3 + 1) * h));
        for (int y = h - 1; y >= 0; --y) // GL rows are bottom-up
        {
            raw.push_back(0);
            raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(y * w * 3),
                       rgb.begin() + static_cast<std::ptrdiff_t>((y + 1) * w * 3));
        }
        std::vector<unsigned char> z {0x78, 0x01};
        std::uint32_t a = 1, b = 0;
        for (std::size_t off = 0; off < raw.size(); off += 65535)
        {
            const std::size_t len = std::min<std::size_t>(65535, raw.size() - off);
            z.push_back(off + len == raw.size() ? 1 : 0);
            z.push_back(static_cast<unsigned char>(len & 0xFF));
            z.push_back(static_cast<unsigned char>(len >> 8));
            z.push_back(static_cast<unsigned char>(~len & 0xFF));
            z.push_back(static_cast<unsigned char>((~len >> 8) & 0xFF));
            z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(off),
                     raw.begin() + static_cast<std::ptrdiff_t>(off + len));
        }
        for (unsigned char c : raw)
        {
            a = (a + c) % 65521u;
            b = (b + a) % 65521u;
        }
        const std::uint32_t adler = (b << 16) | a;
        for (int s = 24; s >= 0; s -= 8)
            z.push_back(static_cast<unsigned char>(adler >> s));

        std::ofstream out(path, std::ios::binary);
        const unsigned char sig[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        out.write(reinterpret_cast<const char*>(sig), 8);
        const auto chunk = [&](const char* type, const std::vector<unsigned char>& data) {
            const std::uint32_t len = static_cast<std::uint32_t>(data.size());
            const unsigned char l[] = {static_cast<unsigned char>(len >> 24), static_cast<unsigned char>(len >> 16),
                                       static_cast<unsigned char>(len >> 8), static_cast<unsigned char>(len)};
            out.write(reinterpret_cast<const char*>(l), 4);
            std::vector<unsigned char> body(type, type + 4);
            body.insert(body.end(), data.begin(), data.end());
            out.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size()));
            const std::uint32_t c = crc32(body.data(), body.size()) ^ 0xFFFFFFFFu;
            const unsigned char cc[] = {static_cast<unsigned char>(c >> 24), static_cast<unsigned char>(c >> 16),
                                        static_cast<unsigned char>(c >> 8), static_cast<unsigned char>(c)};
            out.write(reinterpret_cast<const char*>(cc), 4);
        };
        std::vector<unsigned char> ihdr = {
            static_cast<unsigned char>(w >> 24), static_cast<unsigned char>(w >> 16), static_cast<unsigned char>(w >> 8),
            static_cast<unsigned char>(w), static_cast<unsigned char>(h >> 24), static_cast<unsigned char>(h >> 16),
            static_cast<unsigned char>(h >> 8), static_cast<unsigned char>(h), 8, 2, 0, 0, 0};
        chunk("IHDR", ihdr);
        chunk("IDAT", z);
        chunk("IEND", {});
    }

    // ---- shaders ------------------------------------------------------------
    const char* TerrainVs = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aAlbedo;
layout(location=3) in vec4 aDetail;
layout(location=4) in vec2 aEmissionWater;  // emission, coast (<0 water)
layout(location=5) in vec3 aWaterAlbedo;
uniform mat4 uViewProj;
uniform vec3 uOffset;        // patch origin - camera
uniform vec3 uDetailOffset;  // patch origin mod 1024 m
out vec3 vRel; out vec3 vNormal; out vec3 vAlbedo; out vec4 vDetail; out vec2 vEW; out vec3 vDetailPos; out float vLogz;
out vec3 vWaterAlbedo;
void main(){
  vWaterAlbedo = aWaterAlbedo;
  vRel = aPos + uOffset;
  vNormal = aNormal; vAlbedo = aAlbedo; vDetail = aDetail; vEW = aEmissionWater;
  vDetailPos = aPos + uDetailOffset;
  gl_Position = uViewProj * vec4(vRel, 1.0);
  vLogz = 1.0 + gl_Position.w;
})";

    const char* TerrainFs = R"(#version 330 core
in vec3 vRel; in vec3 vNormal; in vec3 vAlbedo; in vec4 vDetail; in vec2 vEW; in vec3 vDetailPos; in float vLogz;
in vec3 vWaterAlbedo;
uniform vec3 uSun; uniform vec3 uSky; uniform float uFogDensity; uniform float uFcoefHalf;
uniform float uPixelAngle; uniform vec3 uPlanetUp; uniform int uWire;
out vec4 frag;
// Value noise with derivative, periodic over 1024 m (seamless across patches).
float hash(ivec3 p, int period){
  p = ((p % period) + period) % period;
  uint h = uint(p.x) * 374761393u + uint(p.y) * 668265263u + uint(p.z) * 2246822519u;
  h = (h ^ (h >> 13u)) * 1274126177u; h ^= h >> 16u;
  return float(h & 0xFFFFu) / 65535.0;
}
vec4 noised(vec3 x, int period){
  ivec3 i = ivec3(floor(x)); vec3 f = fract(x);
  vec3 u = f*f*f*(f*(f*6.0-15.0)+10.0); vec3 du = 30.0*f*f*(f*(f-2.0)+1.0);
  float a=hash(i,period), b=hash(i+ivec3(1,0,0),period), c=hash(i+ivec3(0,1,0),period), d=hash(i+ivec3(1,1,0),period);
  float e=hash(i+ivec3(0,0,1),period), f1=hash(i+ivec3(1,0,1),period), g=hash(i+ivec3(0,1,1),period), h=hash(i+ivec3(1,1,1),period);
  float k0=a, k1=b-a, k2=c-a, k3=e-a, k4=a-b-c+d, k5=a-c-e+g, k6=a-b-e+f1, k7=-a+b+c-d+e-f1-g+h;
  float v = k0 + k1*u.x + k2*u.y + k3*u.z + k4*u.x*u.y + k5*u.y*u.z + k6*u.z*u.x + k7*u.x*u.y*u.z;
  vec3 g3 = du * vec3(k1 + k4*u.y + k6*u.z + k7*u.y*u.z, k2 + k5*u.z + k4*u.x + k7*u.z*u.x, k3 + k6*u.x + k5*u.y + k7*u.x*u.y);
  return vec4(v*2.0-1.0, g3*2.0);
}
void main(){
  if (uWire == 1) { frag = vec4(0.4,0.9,0.5,1.0); gl_FragDepth = log2(vLogz)*uFcoefHalf; return; }
  vec3 n = normalize(vNormal);
  float dist = length(vRel);
  // The coastline per pixel: the sign of the interpolated macro height.
  float water = vEW.y < 0.0 ? 1.0 : 0.0;
  vec3 albedo = water > 0.5 ? vWaterAlbedo : vAlbedo;
  if (water < 0.5) {
    // Material detail: rock cracks / grain / snow / vegetation clumps, only
    // where the scale is resolved on screen (fades with the pixel footprint).
    float footprint = max(dist * uPixelAngle, 1e-4);
    float rock = vDetail.x, loose = vDetail.y, ice = vDetail.z, veg = vDetail.w;
    float amp = rock*0.9 + loose*0.45 + ice*0.18 + veg*0.6;
    vec3 grad = vec3(0.0); float var = 0.0;
    float scales[4] = float[4](16.0, 4.0, 1.0, 0.25);
    for (int s = 0; s < 4; ++s) {
      float sc = scales[s];
      float w = clamp(sc / (3.0*footprint) - 1.0, 0.0, 1.0);
      if (w <= 0.0) continue;
      int period = int(1024.0 / sc);
      vec4 nd = noised(vDetailPos / sc, period);
      float r = rock * (1.0 - abs(nd.x)) * 0.8 + (1.0 - rock) * nd.x; // ridged on rock
      var += r * w * (s < 2 ? 0.6 : 0.4);
      grad += nd.yzw * w * amp * (s < 2 ? 0.8 : 0.5);
    }
    vec3 tangentGrad = grad - n * dot(grad, n);
    n = normalize(n - tangentGrad * 0.12);
    albedo *= clamp(1.0 + var * amp * 0.30, 0.55, 1.45);
  }
  float diffuse = max(dot(n, uSun), 0.0);
  // Soft terminator on the planet scale.
  float terminator = smoothstep(-0.15, 0.1, dot(normalize(uPlanetUp), uSun));
  vec3 color = albedo * (0.025 + diffuse * 1.15) * mix(0.6, 1.0, terminator);
  if (water > 0.5) {
    vec3 v = normalize(-vRel);
    vec3 h = normalize(v + uSun);
    color += vec3(0.9,0.85,0.7) * pow(max(dot(n,h),0.0), 180.0) * 0.8 * diffuse;
  }
  color += albedo * vEW.x * vec3(2.4,0.9,0.3);
  if (uFogDensity > 0.0) {
    float f = 1.0 - exp(-dist * uFogDensity);
    color = mix(color, uSky * (0.15 + 0.85*max(dot(normalize(uPlanetUp), uSun), 0.0)), f);
  }
  frag = vec4(pow(max(color, vec3(0.0)), vec3(1.0/2.2)), 1.0);
  gl_FragDepth = log2(vLogz) * uFcoefHalf;
})";

    const char* ShellVs = R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in float aAlpha;
uniform mat4 uViewProj; uniform vec3 uCentre;
out vec3 vNormal; out float vAlpha; out vec3 vRel; out float vLogz;
void main(){ vNormal = normalize(aPos); vAlpha = aAlpha; vRel = aPos + uCentre;
  gl_Position = uViewProj * vec4(vRel,1.0); vLogz = 1.0 + gl_Position.w; })";

    const char* CloudFs = R"(#version 330 core
in vec3 vNormal; in float vAlpha; in vec3 vRel; in float vLogz;
uniform vec3 uSun; uniform float uFcoefHalf; out vec4 frag;
void main(){ float d = max(dot(normalize(vNormal), uSun), 0.0);
  vec3 c = vec3(0.97) * (0.04 + 1.1*d);
  frag = vec4(pow(c, vec3(1.0/2.2)), vAlpha);
  gl_FragDepth = log2(vLogz) * uFcoefHalf; })";

    const char* AtmosphereFs = R"(#version 330 core
in vec3 vNormal; in float vAlpha; in vec3 vRel; in float vLogz;
uniform vec3 uSun; uniform vec3 uSky; uniform float uFcoefHalf; uniform float uInside; out vec4 frag;
void main(){ vec3 n = normalize(vNormal); vec3 v = normalize(-vRel);
  float rim = pow(1.0 - abs(dot(n, v)), 3.0);
  float lit = smoothstep(-0.25, 0.3, dot(n, uSun));
  float k = mix(rim * 1.4, 0.9, uInside) * lit;
  frag = vec4(pow(uSky * k, vec3(1.0/2.2)), 1.0);
  gl_FragDepth = log2(vLogz) * uFcoefHalf; })";

    GLuint compile(GLenum type, const char* src)
    {
        const GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[4096];
            glGetShaderInfoLog(s, sizeof log, nullptr, log);
            std::fprintf(stderr, "shader: %s\n", log);
        }
        return s;
    }

    GLuint program(const char* vs, const char* fs)
    {
        const GLuint p = glCreateProgram();
        const GLuint a = compile(GL_VERTEX_SHADER, vs);
        const GLuint b = compile(GL_FRAGMENT_SHADER, fs);
        glAttachShader(p, a);
        glAttachShader(p, b);
        glLinkProgram(p);
        glDeleteShader(a);
        glDeleteShader(b);
        return p;
    }

    // ---- viewer state -----------------------------------------------------------
    enum class View { FarOrbit, LowOrbit, Regional, Approach, Ground, Count };
    const char* viewName(View v)
    {
        static const char* names[] = {"far_orbit", "low_orbit", "regional", "approach", "ground"};
        return names[static_cast<int>(v)];
    }

    struct GpuPatch
    {
        GLuint vao = 0, vbo = 0, ibo = 0;
        GLsizei count = 0;
        std::shared_ptr<const SurfacePatchData> data;
        int lastUsed = 0;
    };

    struct Shell
    {
        GLuint vao = 0, vbo = 0, ibo = 0;
        GLsizei count = 0;
        int n = 64;
        std::vector<glm::dvec3> dirs;
        std::vector<std::uint32_t> indices;
    };

    class Viewer
    {
    public:
        Viewer(const std::vector<LabBody>& bodies, int width, int height) : m_bodies(bodies), m_w(width), m_h(height) {}
        ~Viewer();

        void init();
        void selectBody(std::size_t index);
        void setView(View v);
        void frame(double dtSeconds, bool capture);
        bool converged() const;
        void readPixels(std::vector<unsigned char>& rgb);
        void telemetry(bool force);
        std::string title() const;

        // input
        void onKey(int key);
        void onMouse(double dx, double dy, bool left, bool right);
        void onScroll(double y);
        void setLodCap(int cap) { m_lodCap = cap; }

        const LabBody& body() const { return m_bodies[m_bodyIndex]; }
        View view() const { return m_view; }
        GLuint captureFbo() const { return m_fbo; }

    private:
        struct CameraFrame
        {
            glm::dvec3 position;
            glm::dvec3 forward;
            glm::dvec3 up;
        };
        CameraFrame camera() const;
        double surfaceHeight(const glm::dvec3& dir) const;
        void select(const PatchKey& key, const CameraFrame& cam, const glm::mat4& viewProj,
                    std::vector<PatchKey>& requests, std::vector<const GpuPatch*>& draw,
                    std::unordered_set<PatchKey, PatchKeyHash>& keep, int maxLevel);
        void upload();
        void buildShell(Shell& shell, double radius, const std::vector<float>& alpha);
        void updateClouds(bool force);
        void freeGpu();
        double minimumAltitude() const;

        const std::vector<LabBody>& m_bodies;
        int m_w, m_h;
        std::size_t m_bodyIndex = 0;
        View m_view = View::FarOrbit;
        glm::dvec3 m_focus {0, 0, 1};
        double m_heading = 0.0;
        double m_pitchOffset = 0.0;
        double m_altitude = 1.0e7;
        double m_groundHeight = 0.0;
        int m_frame = 0;
        int m_lodCap = -1;
        bool m_clouds = true;
        bool m_wire = false;
        bool m_timeRunning = false;
        double m_weatherTime = 0.0;

        std::unique_ptr<SurfacePatchStreamer> m_streamer;
        std::unordered_map<PatchKey, GpuPatch, PatchKeyHash> m_gpu;
        int m_maxLevel = 10;
        std::size_t m_drawn = 0;
        int m_drawnMaxLevel = 0;
        int m_uploadsLastFrame = 0;
        int m_quietFrames = 0;

        GLuint m_terrain = 0, m_cloudProgram = 0, m_atmosphereProgram = 0;
        Shell m_cloudShell, m_atmosphereShell;
        std::future<std::vector<float>> m_cloudJob;
        double m_cloudJobTime = -1.0;
        double m_cloudShownTime = -1.0;
        bool m_cloudsReady = false;

        GLuint m_fbo = 0, m_fboColor = 0, m_fboDepth = 0;
        double m_telemetryClock = 0.0;
        std::uint64_t m_generatedAtTelemetry = 0;
        double m_frameMs = 0.0;
    };

    Viewer::~Viewer()
    {
        freeGpu();
    }

    void Viewer::init()
    {
        m_terrain = program(TerrainVs, TerrainFs);
        m_cloudProgram = program(ShellVs, CloudFs);
        m_atmosphereProgram = program(ShellVs, AtmosphereFs);
        glGenFramebuffers(1, &m_fbo);
        glGenTextures(1, &m_fboColor);
        glBindTexture(GL_TEXTURE_2D, m_fboColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_w, m_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &m_fboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_fboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, m_w, m_h);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboColor, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_fboDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Viewer::freeGpu()
    {
        for (auto& [k, p] : m_gpu)
        {
            glDeleteVertexArrays(1, &p.vao);
            glDeleteBuffers(1, &p.vbo);
            glDeleteBuffers(1, &p.ibo);
        }
        m_gpu.clear();
        for (Shell* s : {&m_cloudShell, &m_atmosphereShell})
        {
            if (s->vao)
            {
                glDeleteVertexArrays(1, &s->vao);
                glDeleteBuffers(1, &s->vbo);
                glDeleteBuffers(1, &s->ibo);
            }
            *s = Shell {};
        }
    }

    void Viewer::selectBody(std::size_t index)
    {
        if (m_cloudJob.valid())
            m_cloudJob.wait();
        m_streamer.reset();
        freeGpu();
        m_bodyIndex = index % m_bodies.size();
        const LabBody& b = body();
        const int threads = std::max(2, static_cast<int>(std::thread::hardware_concurrency()) - 2);
        m_streamer = std::make_unique<SurfacePatchStreamer>(b.field, Resolution, threads);
        m_maxLevel = maximumPatchLevel(b.field->referenceRadiusM(), b.policy.finestVertexSpacingM, Resolution);
        m_focus = b.focusDir;
        m_heading = 0.0;
        m_pitchOffset = 0.0;
        m_cloudsReady = !b.clouds;
        m_cloudShownTime = -1.0;
        m_cloudJobTime = -1.0;
        setView(m_view);
        // Atmosphere shell (bodies with an atmosphere).
        const double bar = b.isSmallBody ? 0.0 : b.descriptor.surfacePressurePa / 1.0e5;
        if (bar > 0.003 || !b.field->solidSurface())
            buildShell(m_atmosphereShell, b.field->referenceRadiusM() * (b.field->solidSurface() ? 1.012 : 1.006),
                       {});
        updateClouds(true);
    }

    double Viewer::surfaceHeight(const glm::dvec3& dir) const
    {
        const SurfaceSample s = body().field->sample(dir, 0.5);
        return s.heightM;
    }

    double Viewer::minimumAltitude() const
    {
        const LabBody& b = body();
        if (!b.policy.solidSurface)
            return 100000.0; // above the deck
        if (!b.policy.groundReachable)
            return 2000.0;   // overflight only
        return 1.7;
    }

    void Viewer::setView(View v)
    {
        m_view = v;
        const LabBody& b = body();
        const double r = b.field->referenceRadiusM();
        const bool small = b.isSmallBody;
        switch (v)
        {
            case View::FarOrbit: m_altitude = r * (small ? 3.0 : 2.2); break;
            case View::LowOrbit: m_altitude = r * (small ? 0.8 : 0.3); break;
            case View::Regional: m_altitude = small ? r * 0.25 : std::min(80000.0, r * 0.02); break;
            case View::Approach: m_altitude = small ? r * 0.04 : std::min(3000.0, r * 0.002); break;
            default: m_altitude = small ? 1.6 : 1.8; break;
        }
        m_altitude = std::max(m_altitude, minimumAltitude());
        m_pitchOffset = 0.0;
        m_groundHeight = surfaceHeight(m_focus);
    }

    Viewer::CameraFrame Viewer::camera() const
    {
        const double r = body().field->referenceRadiusM();
        const glm::dvec3 up = m_focus;
        const glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), up) + glm::dvec3(1e-9, 0, 0));
        const glm::dvec3 north = glm::cross(up, east);
        const glm::dvec3 heading = north * std::cos(m_heading) + east * std::sin(m_heading);
        CameraFrame c;
        c.position = up * (r + std::max(0.0, m_groundHeight) + m_altitude);
        if (!body().policy.solidSurface || body().isSmallBody)
            c.position = up * (r + m_groundHeight + m_altitude);
        // High: look at the centre; low: look ahead, nearly level on the ground.
        const double t = std::clamp(std::log(m_altitude / 2.0) / std::log(r * 0.25 / 2.0), 0.0, 1.0);
        double pitch = -(8.0 + 82.0 * t * t) * Pi / 180.0 + m_pitchOffset;
        pitch = std::clamp(pitch, -89.9 * Pi / 180.0, 30.0 * Pi / 180.0);
        c.forward = glm::normalize(heading * std::cos(pitch) + up * std::sin(pitch));
        c.up = glm::normalize(glm::cross(glm::cross(c.forward, up), c.forward));
        return c;
    }

    void Viewer::buildShell(Shell& shell, double radius, const std::vector<float>& alpha)
    {
        const int n = shell.n;
        if (shell.dirs.empty())
        {
            for (int f = 0; f < 6; ++f)
                for (int j = 0; j <= n; ++j)
                    for (int i = 0; i <= n; ++i)
                        shell.dirs.push_back(patchDirection(PatchKey {f, 0, 0, 0}, i, j, n));
            for (int f = 0; f < 6; ++f)
                for (int j = 0; j < n; ++j)
                    for (int i = 0; i < n; ++i)
                    {
                        const auto a = static_cast<std::uint32_t>(f * (n + 1) * (n + 1) + j * (n + 1) + i);
                        shell.indices.insert(shell.indices.end(),
                                             {a, a + 1, a + n + 2, a, a + n + 2, a + static_cast<std::uint32_t>(n + 1)});
                    }
        }
        std::vector<float> data;
        data.reserve(shell.dirs.size() * 4);
        for (std::size_t i = 0; i < shell.dirs.size(); ++i)
        {
            const glm::vec3 p(shell.dirs[i] * radius);
            data.insert(data.end(), {p.x, p.y, p.z, alpha.empty() ? 1.0f : alpha[i]});
        }
        if (!shell.vao)
        {
            glGenVertexArrays(1, &shell.vao);
            glGenBuffers(1, &shell.vbo);
            glGenBuffers(1, &shell.ibo);
            glBindVertexArray(shell.vao);
            glBindBuffer(GL_ARRAY_BUFFER, shell.vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shell.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(shell.indices.size() * 4),
                         shell.indices.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void*>(12));
            shell.count = static_cast<GLsizei>(shell.indices.size());
        }
        glBindVertexArray(shell.vao);
        glBindBuffer(GL_ARRAY_BUFFER, shell.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * 4), data.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(0);
    }

    void Viewer::updateClouds(bool force)
    {
        const LabBody& b = body();
        if (!b.clouds)
            return;
        if (m_cloudJob.valid() && m_cloudJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            const std::vector<float> alpha = m_cloudJob.get();
            buildShell(m_cloudShell, b.field->referenceRadiusM() + b.clouds->layers().front().heightM, alpha);
            m_cloudShownTime = m_cloudJobTime;
            m_cloudsReady = true;
        }
        if (m_cloudJob.valid())
            return;
        if (!force && std::abs(m_weatherTime - m_cloudShownTime) < 600.0)
            return;
        // Deterministic weather at the current world time, off the frame.
        if (m_cloudShell.dirs.empty())
            buildShell(m_cloudShell, b.field->referenceRadiusM(), {});
        m_cloudJobTime = m_weatherTime;
        auto clouds = b.clouds;
        auto dirs = m_cloudShell.dirs;
        const double t = m_weatherTime;
        m_cloudJob = std::async(std::launch::async, [clouds, dirs, t] {
            std::vector<float> alpha(dirs.size());
            for (std::size_t i = 0; i < dirs.size(); ++i)
            {
                float a = 0.0f;
                for (std::size_t layer = 0; layer < clouds->layers().size(); ++layer)
                    a = 1.0f - (1.0f - a) * (1.0f - clouds->coverage(layer, dirs[i], t));
                alpha[i] = a;
            }
            return alpha;
        });
    }

    void Viewer::upload()
    {
        m_uploadsLastFrame = 0;
        for (const auto& patch : m_streamer->takeCompleted())
        {
            GpuPatch& g = m_gpu[patch->key];
            if (!g.vao)
            {
                glGenVertexArrays(1, &g.vao);
                glGenBuffers(1, &g.vbo);
                glGenBuffers(1, &g.ibo);
            }
            g.data = patch;
            glBindVertexArray(g.vao);
            glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(patch->vertices.size() * sizeof(PatchVertex)),
                         patch->vertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(patch->indices.size() * 4),
                         patch->indices.data(), GL_STATIC_DRAW);
            const GLsizei stride = sizeof(PatchVertex);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PatchVertex, position)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PatchVertex, normal)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PatchVertex, albedo)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PatchVertex, detail)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PatchVertex, emission)));
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PatchVertex, waterAlbedo)));
            g.count = static_cast<GLsizei>(patch->indices.size());
            ++m_uploadsLastFrame;
        }
        glBindVertexArray(0);
    }

    void Viewer::select(const PatchKey& key, const CameraFrame& cam, const glm::mat4& viewProj,
                        std::vector<PatchKey>& requests, std::vector<const GpuPatch*>& draw,
                        std::unordered_set<PatchKey, PatchKeyHash>& keep, int maxLevel)
    {
        const SurfaceField& field = *body().field;
        const double r = field.referenceRadiusM();
        keep.insert(key);
        const auto it = m_gpu.find(key);
        const GpuPatch* gpu = it != m_gpu.end() ? &it->second : nullptr;
        const double edge = patchEdgeLengthM(key.level, r);
        glm::dvec3 centre = patchDirection(key, Resolution / 2, Resolution / 2, Resolution) * r;
        double radius = edge * 0.8 + field.detailAmplitudeBoundM() + 20000.0 * (key.level < 6);
        if (gpu && gpu->data)
        {
            centre = gpu->data->origin;
            radius = gpu->data->boundingRadiusM;
        }
        // Horizon: the patch's cap is entirely behind the planet's limb.
        const double camR = glm::length(cam.position);
        if (camR > r * 1.001 && key.level > 1)
        {
            const double horizon = std::acos(std::clamp(r * 0.995 / camR, -1.0, 1.0));
            const double angle = std::acos(std::clamp(glm::dot(glm::normalize(centre), glm::normalize(cam.position)),
                                                      -1.0, 1.0));
            if (angle - radius / r > horizon + 0.02)
                return;
        }
        // Frustum: the bounding sphere against the four side planes
        // (Gribb-Hartmann, normalised; camera-relative metres).
        {
            const glm::dvec3 rel = centre - cam.position;
            const auto row = [&](int r) {
                return glm::dvec4(viewProj[0][r], viewProj[1][r], viewProj[2][r], viewProj[3][r]);
            };
            const glm::dvec4 planes[4] = {row(3) + row(0), row(3) - row(0), row(3) + row(1), row(3) - row(1)};
            for (const glm::dvec4& p : planes)
            {
                const double len = glm::length(glm::dvec3(p));
                if ((glm::dot(glm::dvec3(p), rel) + p.w) / len < -radius * 1.1)
                    return;
            }
        }
        // Screen-space error: split while a vertex spacing covers > 5 px.
        const double distance = std::max(1.0e-3, glm::length(centre - cam.position) - radius);
        const double spacingPx = edge / Resolution / distance * (m_h / FovY);
        const bool split = key.level < maxLevel && spacingPx > 5.0;
        if (split)
        {
            bool childrenReady = true;
            for (int i = 0; i < 4; ++i)
                childrenReady = childrenReady && m_gpu.count(key.child(i));
            if (childrenReady)
            {
                for (int i = 0; i < 4; ++i)
                    select(key.child(i), cam, viewProj, requests, draw, keep, maxLevel);
                return;
            }
            for (int i = 0; i < 4; ++i)
            {
                requests.push_back(key.child(i));
                keep.insert(key.child(i));
            }
        }
        if (gpu)
            draw.push_back(gpu);
        else
            requests.push_back(key);
    }

    void Viewer::frame(double dt, bool capture)
    {
        const auto t0 = std::chrono::steady_clock::now();
        ++m_frame;
        if (m_timeRunning)
            m_weatherTime += dt * 3600.0;
        updateClouds(false);
        upload();

        const LabBody& b = body();
        const SurfaceField& field = *b.field;
        const double r = field.referenceRadiusM();
        const CameraFrame cam = camera();
        const double nearPlane = 0.05;
        const glm::mat4 proj = glm::perspective(static_cast<float>(FovY), static_cast<float>(m_w) / m_h,
                                                static_cast<float>(nearPlane), static_cast<float>(FarPlane));
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(cam.forward), glm::vec3(cam.up));
        const glm::mat4 viewProj = proj * view;

        std::vector<PatchKey> requests;
        std::vector<const GpuPatch*> draw;
        std::unordered_set<PatchKey, PatchKeyHash> keep;
        const int maxLevel = m_lodCap >= 0 ? std::min(m_lodCap, m_maxLevel) : m_maxLevel;
        for (int f = 0; f < 6; ++f)
            select(PatchKey {f, 0, 0, 0}, cam, viewProj, requests, draw, keep, maxLevel);
        // Coarse first, then near first.
        std::stable_sort(requests.begin(), requests.end(), [&](const PatchKey& a, const PatchKey& c) {
            if (a.level != c.level)
                return a.level < c.level;
            const double da = glm::length(patchDirection(a, 16, 16, 32) * r - cam.position);
            const double dc = glm::length(patchDirection(c, 16, 16, 32) * r - cam.position);
            return da < dc;
        });
        m_streamer->setRequests(requests);
        for (const GpuPatch* g : draw)
            const_cast<GpuPatch*>(g)->lastUsed = m_frame;
        if (m_frame % 60 == 0)
        {
            for (auto it = m_gpu.begin(); it != m_gpu.end();)
            {
                if (!keep.count(it->first) && m_frame - it->second.lastUsed > 240)
                {
                    glDeleteVertexArrays(1, &it->second.vao);
                    glDeleteBuffers(1, &it->second.vbo);
                    glDeleteBuffers(1, &it->second.ibo);
                    it = m_gpu.erase(it);
                }
                else
                    ++it;
            }
            std::unordered_set<PatchKey, PatchKeyHash> resident;
            for (const auto& [k, g] : m_gpu)
                resident.insert(k);
            m_streamer->retain(resident);
        }

        // Sky / fog for bodies with an atmosphere.
        const double bar = b.isSmallBody ? 0.0 : b.descriptor.surfacePressurePa / 1.0e5;
        glm::vec3 sky(0.30f, 0.52f, 0.95f);
        if (!b.isSmallBody && b.descriptor.bodyClass != "rocky" && field.solidSurface())
            sky = glm::vec3(0.75f, 0.55f, 0.40f);
        if (b.label == "dry")
            sky = glm::vec3(0.78f, 0.58f, 0.42f);
        if (!field.solidSurface())
            sky = glm::vec3(0.55f, 0.65f, 0.85f);
        // Sun 55 deg from the focus zenith (from the east): relief is lit
        // obliquely, the terminator visible from orbit.
        const glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), m_focus) + glm::dvec3(1e-9, 0, 0));
        const glm::dvec3 sun = glm::normalize(m_focus * std::cos(0.96) + east * std::sin(0.96));
        const double altitude = glm::length(cam.position) - r;
        const double scaleHeight = 8000.0 * std::max(0.2, std::min(2.0, std::sqrt(std::max(bar, 0.0))));
        const float fog = bar > 0.003 && field.solidSurface()
                              ? static_cast<float>(std::min(1.0, bar) * 1.0 / 250000.0 *
                                                   std::exp(-std::max(0.0, altitude) / (scaleHeight * 3.0)))
                              : 0.0f;
        const bool insideAtmosphere = bar > 0.003 && altitude < scaleHeight * 6.0;
        const float daylight = static_cast<float>(std::max(0.0, glm::dot(m_focus, sun)));
        glm::vec3 clear = insideAtmosphere ? sky * (0.25f + 0.75f * daylight) *
                                                 static_cast<float>(1.0 - altitude / (scaleHeight * 6.0))
                                           : glm::vec3(0.0f);
        clear = glm::pow(clear, glm::vec3(1.0f / 2.2f));

        glBindFramebuffer(GL_FRAMEBUFFER, capture ? m_fbo : 0);
        glViewport(0, 0, m_w, m_h);
        glClearColor(clear.r, clear.g, clear.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);
        const float fcoefHalf = static_cast<float>(1.0 / std::log2(FarPlane + 1.0));

        glUseProgram(m_terrain);
        glUniformMatrix4fv(glGetUniformLocation(m_terrain, "uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
        glUniform3fv(glGetUniformLocation(m_terrain, "uSun"), 1, glm::value_ptr(glm::vec3(sun)));
        glUniform3fv(glGetUniformLocation(m_terrain, "uSky"), 1, glm::value_ptr(sky));
        glUniform1f(glGetUniformLocation(m_terrain, "uFogDensity"), fog);
        glUniform1f(glGetUniformLocation(m_terrain, "uFcoefHalf"), fcoefHalf);
        glUniform1f(glGetUniformLocation(m_terrain, "uPixelAngle"), static_cast<float>(FovY / m_h));
        glUniform3fv(glGetUniformLocation(m_terrain, "uPlanetUp"), 1, glm::value_ptr(glm::vec3(m_focus)));
        glUniform1i(glGetUniformLocation(m_terrain, "uWire"), m_wire ? 1 : 0);
        const GLint offsetLoc = glGetUniformLocation(m_terrain, "uOffset");
        const GLint detailLoc = glGetUniformLocation(m_terrain, "uDetailOffset");
        const GLint upLoc = glGetUniformLocation(m_terrain, "uPlanetUp");
        if (m_wire)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        m_drawn = draw.size();
        m_drawnMaxLevel = 0;
        for (const GpuPatch* g : draw)
        {
            const glm::dvec3 o = g->data->origin;
            const glm::vec3 offset(o - cam.position);
            const glm::dvec3 period(1024.0);
            const glm::vec3 detail(o - glm::floor(o / period) * period);
            glUniform3fv(offsetLoc, 1, glm::value_ptr(offset));
            glUniform3fv(detailLoc, 1, glm::value_ptr(detail));
            glUniform3fv(upLoc, 1, glm::value_ptr(glm::vec3(glm::normalize(o))));
            glBindVertexArray(g->vao);
            glDrawElements(GL_TRIANGLES, g->count, GL_UNSIGNED_INT, nullptr);
            m_drawnMaxLevel = std::max(m_drawnMaxLevel, g->data->key.level);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        const glm::vec3 centre(-cam.position);
        if (m_clouds && b.clouds && m_cloudsReady && m_cloudShell.vao)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glUseProgram(m_cloudProgram);
            glUniformMatrix4fv(glGetUniformLocation(m_cloudProgram, "uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
            glUniform3fv(glGetUniformLocation(m_cloudProgram, "uCentre"), 1, glm::value_ptr(centre));
            glUniform3fv(glGetUniformLocation(m_cloudProgram, "uSun"), 1, glm::value_ptr(glm::vec3(sun)));
            glUniform1f(glGetUniformLocation(m_cloudProgram, "uFcoefHalf"), fcoefHalf);
            glBindVertexArray(m_cloudShell.vao);
            glDrawElements(GL_TRIANGLES, m_cloudShell.count, GL_UNSIGNED_INT, nullptr);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        if (m_atmosphereShell.vao)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT); // the far side of the shell: limb glow / sky
            glUseProgram(m_atmosphereProgram);
            glUniformMatrix4fv(glGetUniformLocation(m_atmosphereProgram, "uViewProj"), 1, GL_FALSE,
                               glm::value_ptr(viewProj));
            glUniform3fv(glGetUniformLocation(m_atmosphereProgram, "uCentre"), 1, glm::value_ptr(centre));
            glUniform3fv(glGetUniformLocation(m_atmosphereProgram, "uSun"), 1, glm::value_ptr(glm::vec3(sun)));
            glUniform3fv(glGetUniformLocation(m_atmosphereProgram, "uSky"), 1, glm::value_ptr(sky * 0.6f));
            glUniform1f(glGetUniformLocation(m_atmosphereProgram, "uFcoefHalf"), fcoefHalf);
            glUniform1f(glGetUniformLocation(m_atmosphereProgram, "uInside"), insideAtmosphere ? 1.0f : 0.0f);
            glBindVertexArray(m_atmosphereShell.vao);
            if (!insideAtmosphere)
                glDrawElements(GL_TRIANGLES, m_atmosphereShell.count, GL_UNSIGNED_INT, nullptr);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        glBindVertexArray(0);

        const auto stats = m_streamer->stats();
        m_quietFrames = (stats.pending == 0 && m_uploadsLastFrame == 0 && requests.empty()) ? m_quietFrames + 1 : 0;
        m_frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        (void)altitude;
    }

    bool Viewer::converged() const
    {
        return m_quietFrames > 5 && m_cloudsReady && !m_cloudJob.valid();
    }

    void Viewer::readPixels(std::vector<unsigned char>& rgb)
    {
        rgb.resize(static_cast<std::size_t>(m_w * m_h * 3));
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, m_w, m_h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    std::string Viewer::title() const
    {
        const auto s = m_streamer->stats();
        char buffer[512];
        std::snprintf(buffer, sizeof buffer,
                      "Planet Lab - %s [%s] %s | alt %.0f m | drawn %zu (max L%d / cap %d) resident %zu pending %zu | "
                      "gen %llu avg %.1f ms | weather t=%.1f h | frame %.1f ms",
                      body().label.c_str(), viewName(m_view), body().policy.reason.c_str(), m_altitude, m_drawn,
                      m_drawnMaxLevel, m_lodCap >= 0 ? std::min(m_lodCap, m_maxLevel) : m_maxLevel, s.resident,
                      s.pending, static_cast<unsigned long long>(s.generated),
                      s.generated ? s.generationMsTotal / static_cast<double>(s.generated) : 0.0, m_weatherTime / 3600.0,
                      m_frameMs);
        return buffer;
    }

    void Viewer::telemetry(bool force)
    {
        const double now = glfwGetTime();
        if (!force && now - m_telemetryClock < 2.0)
            return;
        const auto s = m_streamer->stats();
        const double rate = (static_cast<double>(s.generated - m_generatedAtTelemetry)) /
                            std::max(1.0e-3, now - m_telemetryClock);
        m_telemetryClock = now;
        m_generatedAtTelemetry = s.generated;
        std::printf("[PlanetLab] %s | %.1f patches/s | max patch %.1f ms\n", title().c_str(), rate, s.generationMsMax);
        std::fflush(stdout);
    }

    void Viewer::onKey(int key)
    {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9 && static_cast<std::size_t>(key - GLFW_KEY_1) < m_bodies.size())
            selectBody(static_cast<std::size_t>(key - GLFW_KEY_1));
        else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F5)
            setView(static_cast<View>(key - GLFW_KEY_F1));
        else if (key == GLFW_KEY_L)
            m_lodCap = m_lodCap < 0 ? 3 : m_lodCap == 3 ? 6 : m_lodCap == 6 ? 9 : -1;
        else if (key == GLFW_KEY_C)
            m_clouds = !m_clouds;
        else if (key == GLFW_KEY_W)
            m_wire = !m_wire;
        else if (key == GLFW_KEY_T)
            m_weatherTime += 6.0 * 3600.0;
        else if (key == GLFW_KEY_SPACE)
            m_timeRunning = !m_timeRunning;
        else if (key == GLFW_KEY_P)
            telemetry(true);
    }

    void Viewer::onMouse(double dx, double dy, bool left, bool right)
    {
        if (left)
        {
            m_heading += dx * 0.004;
            m_pitchOffset = std::clamp(m_pitchOffset - dy * 0.003, -1.4, 1.4);
        }
        if (right)
        {
            const glm::dvec3 up = m_focus;
            const glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), up) + glm::dvec3(1e-9, 0, 0));
            const glm::dvec3 north = glm::cross(up, east);
            const glm::dvec3 heading = north * std::cos(m_heading) + east * std::sin(m_heading);
            const glm::dvec3 side = glm::cross(heading, up);
            const double scale = (m_altitude + 50.0) / body().field->referenceRadiusM() * 0.0025;
            m_focus = glm::normalize(m_focus + (heading * dy + side * dx) * scale);
            m_groundHeight = surfaceHeight(m_focus);
        }
    }

    void Viewer::onScroll(double y)
    {
        m_altitude = std::clamp(m_altitude * std::pow(1.18, -y), minimumAltitude(),
                                body().field->referenceRadiusM() * 6.0);
    }

    // Glue for the GLFW callbacks.
    Viewer* g_viewer = nullptr;
    double g_scroll = 0.0;
}

int runViewer(const std::vector<LabBody>& bodies, const std::string& captureDir)
{
    const bool capture = !captureDir.empty();
    if (!glfwInit())
    {
        std::fprintf(stderr, "Planet Lab: GLFW init failed\n");
        return 2;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (capture)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    const int width = capture ? 1280 : 1600;
    const int height = capture ? 720 : 900;
    GLFWwindow* window = glfwCreateWindow(width, height, "Planet Lab", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "Planet Lab: window creation failed\n");
        glfwTerminate();
        return 2;
    }
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(capture ? 0 : 1);

    int result = 0;
    {
        Viewer viewer(bodies, width, height);
        g_viewer = &viewer;
        viewer.init();
        viewer.selectBody(0);

        if (capture)
        {
            std::filesystem::create_directories(captureDir);
            std::FILE* log = std::fopen((captureDir + "/planet_lab_capture.txt").c_str(), "w");
            const auto shoot = [&](const std::string& name, double timeoutS) {
                const double start = glfwGetTime();
                double last = start;
                while (glfwGetTime() - start < timeoutS)
                {
                    glfwPollEvents();
                    const double now = glfwGetTime();
                    viewer.frame(now - last, true);
                    last = now;
                    if (viewer.converged())
                        break;
                }
                viewer.frame(0.0, true);
                glFinish();
                std::vector<unsigned char> rgb;
                viewer.readPixels(rgb);
                const std::string path = captureDir + "/planet_lab_" + name + ".png";
                writePng(path, width, height, rgb);
                const std::string line = viewer.title() + " | converged in " +
                                         std::to_string(glfwGetTime() - start) + " s -> " + path;
                std::printf("%s\n", line.c_str());
                if (log)
                    std::fprintf(log, "%s\n", line.c_str());
            };
            // PLANET_LAB_ONLY=<body label>, PLANET_LAB_VIEWS=<digits 0-4>,
            // PLANET_LAB_FLAGS=noclouds|wire: a subset for iteration.
            const char* onlyEnv = std::getenv("PLANET_LAB_ONLY");
            const char* viewsEnv = std::getenv("PLANET_LAB_VIEWS");
            const char* flagsEnv = std::getenv("PLANET_LAB_FLAGS");
            const std::string only = onlyEnv ? onlyEnv : "";
            const std::string views = viewsEnv ? viewsEnv : "01234L";
            const std::string flags = flagsEnv ? flagsEnv : "";
            if (flags.find("noclouds") != std::string::npos)
                viewer.onKey(GLFW_KEY_C);
            if (flags.find("wire") != std::string::npos)
                viewer.onKey(GLFW_KEY_W);
            for (std::size_t b = 0; b < bodies.size(); ++b)
            {
                if (!only.empty() && bodies[b].label != only)
                    continue;
                viewer.selectBody(b);
                for (int v = 0; v < static_cast<int>(View::Count); ++v)
                {
                    if (views.find(static_cast<char>('0' + v)) == std::string::npos)
                        continue;
                    viewer.setView(static_cast<View>(v));
                    shoot(bodies[b].label + "_" + std::to_string(v) + "_" + viewName(static_cast<View>(v)), 60.0);
                }
                // LOD consistency: the same orbit view at LOD caps 3 and 6.
                if (views.find('L') == std::string::npos)
                    continue;
                viewer.setView(View::LowOrbit);
                viewer.setLodCap(3);
                shoot(bodies[b].label + "_lodcap3", 30.0);
                viewer.setLodCap(6);
                shoot(bodies[b].label + "_lodcap6", 30.0);
                viewer.setLodCap(-1);
            }
            if (log)
                std::fclose(log);
        }
        else
        {
            glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int) {
                if (action != GLFW_PRESS)
                    return;
                if (key == GLFW_KEY_ESCAPE)
                    glfwSetWindowShouldClose(w, 1);
                g_viewer->onKey(key);
            });
            glfwSetScrollCallback(window, [](GLFWwindow*, double, double y) { g_scroll += y; });
            double lastX = 0.0, lastY = 0.0;
            glfwGetCursorPos(window, &lastX, &lastY);
            double last = glfwGetTime();
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                double x = 0.0, y = 0.0;
                glfwGetCursorPos(window, &x, &y);
                const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
                viewer.onMouse(x - lastX, y - lastY, left, right);
                lastX = x;
                lastY = y;
                const double ax = (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS);
                const double ay = (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS);
                if (ax != 0.0 || ay != 0.0)
                    viewer.onMouse(ax * 6.0, ay * 6.0, false, true);
                if (g_scroll != 0.0)
                {
                    viewer.onScroll(g_scroll);
                    g_scroll = 0.0;
                }
                const double now = glfwGetTime();
                viewer.frame(now - last, false);
                last = now;
                glfwSetWindowTitle(window, viewer.title().c_str());
                viewer.telemetry(false);
                glfwSwapBuffers(window);
            }
        }
        g_viewer = nullptr;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
} // namespace planet_lab
