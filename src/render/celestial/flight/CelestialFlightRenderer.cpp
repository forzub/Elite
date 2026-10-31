#include "src/render/celestial/flight/CelestialFlightRenderer.h"

#include <glad/gl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/bitmap/stb_image.h"
#include "src/render/celestial/flight/CelestialFlightShaders.h"
#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/CloudWeatherField.h"
#include "src/world/celestial/surface/GlobeBake.h"
#include "src/world/celestial/surface/ImportedSurfaceField.h"
#include "src/world/celestial/surface/LocalTerrain.h"
#include "src/world/celestial/surface/PlanetSurfaceField.h"
#include "src/world/celestial/surface/SmallBodyField.h"

namespace render::celestial_flight
{
namespace surface = world::celestial::surface;

namespace
{
    constexpr double MetresPerAu = 149597870700.0;
    constexpr double LogFarM = 1.0e14;

    double nowSeconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // ---- GL helpers -----------------------------------------------------
    GLuint compileShader(GLenum type, const std::vector<const char*>& parts)
    {
        std::string src = "#version 330 core\n";
        for (const char* p : parts)
            src += p;
        const GLuint s = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(s, 1, &c, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[4096];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            std::fprintf(stderr, "[CelestialFlight] shader compile failed:\n%s\n", log);
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    struct Program
    {
        GLuint id = 0;
        mutable std::unordered_map<std::string, GLint> locations;
        GLint u(const char* name) const
        {
            auto it = locations.find(name);
            if (it != locations.end())
                return it->second;
            const GLint l = glGetUniformLocation(id, name);
            locations.emplace(name, l);
            return l;
        }
        void use() const { glUseProgram(id); }
        void set(const char* n, int v) const { glUniform1i(u(n), v); }
        void set(const char* n, float v) const { glUniform1f(u(n), v); }
        void set(const char* n, const glm::vec2& v) const { glUniform2fv(u(n), 1, glm::value_ptr(v)); }
        void set(const char* n, const glm::vec3& v) const { glUniform3fv(u(n), 1, glm::value_ptr(v)); }
        void set(const char* n, const glm::mat3& v) const { glUniformMatrix3fv(u(n), 1, GL_FALSE, glm::value_ptr(v)); }
        void set(const char* n, const glm::mat4& v) const { glUniformMatrix4fv(u(n), 1, GL_FALSE, glm::value_ptr(v)); }
    };

    bool linkProgram(Program& out, const std::vector<const char*>& vs, const std::vector<const char*>& fs)
    {
        const GLuint v = compileShader(GL_VERTEX_SHADER, vs);
        const GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
        if (!v || !f)
            return false;
        out.id = glCreateProgram();
        glAttachShader(out.id, v);
        glAttachShader(out.id, f);
        glLinkProgram(out.id);
        glDeleteShader(v);
        glDeleteShader(f);
        GLint ok = 0;
        glGetProgramiv(out.id, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[4096];
            glGetProgramInfoLog(out.id, sizeof(log), nullptr, log);
            std::fprintf(stderr, "[CelestialFlight] program link failed:\n%s\n", log);
            return false;
        }
        return true;
    }

    GLuint makeCube(int size, GLenum internal, GLenum format, GLenum type,
                    const std::array<const void*, 6>& faces)
    {
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_CUBE_MAP, t);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int f = 0; f < 6; ++f)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, static_cast<GLint>(internal), size, size, 0, format,
                         type, faces[static_cast<std::size_t>(f)]);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return t;
    }

    glm::vec3 blackbody(double kelvin)
    {
        // Tanner Helland's fit, normalised to the brightest channel.
        const double t = std::clamp(kelvin, 1000.0, 40000.0) / 100.0;
        double r, g, b;
        if (t <= 66.0)
        {
            r = 255.0;
            g = 99.4708025861 * std::log(t) - 161.1195681661;
            b = t <= 19.0 ? 0.0 : 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
        }
        else
        {
            r = 329.698727446 * std::pow(t - 60.0, -0.1332047592);
            g = 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
            b = 255.0;
        }
        glm::vec3 c(static_cast<float>(std::clamp(r, 0.0, 255.0) / 255.0),
                    static_cast<float>(std::clamp(g, 0.0, 255.0) / 255.0),
                    static_cast<float>(std::clamp(b, 0.0, 255.0) / 255.0));
        return c / std::max(0.001f, std::max(c.r, std::max(c.g, c.b)));
    }

    double fraction(const surface::BodySurfaceDescriptor& d, const char* gas)
    {
        for (const auto& [name, value] : d.atmosphereComposition)
            if (name == gas)
                return value;
        return 0.0;
    }

    // ---- worker pool ------------------------------------------------------
    class JobPool
    {
    public:
        void start(int threads)
        {
            m_stop = false;
            for (int i = 0; i < threads; ++i)
                m_threads.emplace_back([this] { run(); });
        }
        void stop()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stop = true;
                while (!m_queue.empty())
                    m_queue.pop();
            }
            m_cv.notify_all();
            for (auto& t : m_threads)
                t.join();
            m_threads.clear();
        }
        // Lower priority value runs first.
        void push(int priority, std::function<void()> job)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue.push(Entry {priority, m_sequence++, std::move(job)});
                ++m_pending;
            }
            m_cv.notify_one();
        }
        int pending() const { return m_pending.load(); }
        double busySeconds() const { return m_busyMicros.load() * 1.0e-6; }

    private:
        struct Entry
        {
            int priority;
            std::uint64_t sequence;
            std::function<void()> job;
            bool operator<(const Entry& o) const
            {
                return priority != o.priority ? priority > o.priority : sequence > o.sequence;
            }
        };
        void run()
        {
            for (;;)
            {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                    if (m_stop)
                        return;
                    job = std::move(const_cast<Entry&>(m_queue.top()).job);
                    m_queue.pop();
                }
                const double t0 = nowSeconds();
                job();
                m_busyMicros += static_cast<long long>((nowSeconds() - t0) * 1.0e6);
                --m_pending;
            }
        }
        std::vector<std::thread> m_threads;
        std::priority_queue<Entry> m_queue;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_stop = false;
        std::uint64_t m_sequence = 0;
        std::atomic<int> m_pending {0};
        std::atomic<long long> m_busyMicros {0};
    };

    // ---- GPU timers (timestamp pairs, read two frames later) -------------
    enum TimerCategory
    {
        TGlobes,
        TTerrain,
        TClouds,
        TAtmosphere,
        TRings,
        TSprites,
        TAsteroids,
        TCount
    };

    class GpuTimers
    {
    public:
        void init()
        {
            for (auto& f : m_frames)
            {
                f.queries.resize(256);
                glGenQueries(static_cast<GLsizei>(f.queries.size()), f.queries.data());
            }
        }
        void destroy()
        {
            for (auto& f : m_frames)
                if (!f.queries.empty())
                    glDeleteQueries(static_cast<GLsizei>(f.queries.size()), f.queries.data());
        }
        void beginFrame()
        {
            m_index = (m_index + 1) % 3;
            Frame& old = m_frames[m_index];
            // Collect the frame recorded three frames ago.
            if (old.used > 0)
            {
                std::array<double, TCount> ms {};
                bool ok = true;
                for (int i = 0; i < old.used && ok; i += 2)
                {
                    GLint available = 0;
                    glGetQueryObjectiv(old.queries[static_cast<std::size_t>(i + 1)], GL_QUERY_RESULT_AVAILABLE,
                                       &available);
                    if (!available)
                        ok = false;
                }
                if (ok)
                {
                    for (int i = 0; i < old.used; i += 2)
                    {
                        GLuint64 a = 0, b = 0;
                        glGetQueryObjectui64v(old.queries[static_cast<std::size_t>(i)], GL_QUERY_RESULT, &a);
                        glGetQueryObjectui64v(old.queries[static_cast<std::size_t>(i + 1)], GL_QUERY_RESULT, &b);
                        ms[static_cast<std::size_t>(old.categories[static_cast<std::size_t>(i / 2)])] +=
                            static_cast<double>(b - a) * 1.0e-6;
                    }
                    m_result = ms;
                }
            }
            old.used = 0;
            old.categories.clear();
        }
        void begin(TimerCategory c)
        {
            Frame& f = m_frames[m_index];
            if (f.used + 2 > static_cast<int>(f.queries.size()))
            {
                m_open = false;
                return;
            }
            glQueryCounter(f.queries[static_cast<std::size_t>(f.used)], GL_TIMESTAMP);
            f.categories.push_back(c);
            m_open = true;
        }
        void end()
        {
            if (!m_open)
                return;
            Frame& f = m_frames[m_index];
            glQueryCounter(f.queries[static_cast<std::size_t>(f.used + 1)], GL_TIMESTAMP);
            f.used += 2;
            m_open = false;
        }
        const std::array<double, TCount>& result() const { return m_result; }

    private:
        struct Frame
        {
            std::vector<GLuint> queries;
            std::vector<TimerCategory> categories;
            int used = 0;
        };
        std::array<Frame, 3> m_frames;
        int m_index = 0;
        bool m_open = false;
        std::array<double, TCount> m_result {};
    };

    struct ScopedTimer
    {
        GpuTimers& t;
        ScopedTimer(GpuTimers& timers, TimerCategory c) : t(timers) { t.begin(c); }
        ~ScopedTimer() { t.end(); }
    };

    // ---- meshes -------------------------------------------------------------
    struct IndexedMesh
    {
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLsizei count = 0;
        std::size_t bytes = 0;
        void destroy()
        {
            if (ebo) glDeleteBuffers(1, &ebo);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (vao) glDeleteVertexArrays(1, &vao);
            vao = vbo = ebo = 0;
            count = 0;
            bytes = 0;
        }
    };

    IndexedMesh makeSphereMesh(int n)
    {
        std::vector<glm::vec3> v;
        std::vector<std::uint32_t> idx;
        for (int f = 0; f < 6; ++f)
        {
            const auto base = static_cast<std::uint32_t>(v.size());
            for (int j = 0; j <= n; ++j)
                for (int i = 0; i <= n; ++i)
                {
                    const double s = 2.0 * i / n - 1.0;
                    const double t = 2.0 * j / n - 1.0;
                    // Equal-angle spacing for uniform triangles.
                    const glm::dvec3 d = surface::cubeMapDirection(f, std::tan(s * 0.78539816339744831),
                                                                    std::tan(t * 0.78539816339744831));
                    v.emplace_back(glm::vec3(d));
                }
            // Counter-clockwise seen from outside, whatever the face frame.
            const glm::vec3 p0 = v[base], p1 = v[base + 1], p2 = v[base + static_cast<std::uint32_t>(n + 1)];
            const bool flip = glm::dot(glm::cross(p1 - p0, p2 - p0), p0) > 0.0f;
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i)
                {
                    const std::uint32_t a = base + static_cast<std::uint32_t>(j * (n + 1) + i);
                    const std::uint32_t b = a + 1;
                    const std::uint32_t c = a + static_cast<std::uint32_t>(n + 1);
                    const std::uint32_t d = c + 1;
                    if (flip)
                        idx.insert(idx.end(), {a, c, b, b, c, d});
                    else
                        idx.insert(idx.end(), {a, b, c, b, d, c});
                }
        }
        IndexedMesh m;
        glGenVertexArrays(1, &m.vao);
        glBindVertexArray(m.vao);
        glGenBuffers(1, &m.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(glm::vec3)), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
        glGenBuffers(1, &m.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * 4), idx.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
        m.count = static_cast<GLsizei>(idx.size());
        m.bytes = v.size() * sizeof(glm::vec3) + idx.size() * 4;
        return m;
    }

    struct SpriteVertex
    {
        glm::vec3 pos;
        glm::vec4 colour;
        float size;
        float kind;
    };

    struct AsteroidVertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 albedo;
    };

    // ---- per-body runtime ------------------------------------------------
    struct LoadResult
    {
        std::atomic<bool> done {false};
        bool ok = false;
        std::string error;
        surface::BodySurfaceDescriptor descriptor;
        std::shared_ptr<surface::SurfaceField> field;
        std::shared_ptr<surface::CloudWeatherField> clouds;
    };


    struct GlobeTier
    {
        int size = 0;
        GLuint albedo = 0, normal = 0, relief = 0;
        std::size_t bytes = 0;
        glm::vec3 meanAlbedo {0.5f};
        void destroy()
        {
            GLuint t[3] = {albedo, normal, relief};
            glDeleteTextures(3, t);
            albedo = normal = relief = 0;
            size = 0;
            bytes = 0;
        }
    };

    struct PendingGlobe
    {
        int size = 0;
        std::shared_ptr<surface::GlobeFaces> faces;
        std::shared_ptr<std::atomic<int>> done;
        std::shared_ptr<std::atomic<bool>> cancel;
        GlobeTier staging;   // textures being filled
        int cursor = 0;      // texture * 6 + face uploaded so far
    };

    struct PendingCloud
    {
        double timeSeconds = 0.0;
        std::shared_ptr<surface::CloudFaces> faces;
        std::shared_ptr<std::atomic<int>> done;
    };

    struct CloudTier
    {
        GLuint current = 0, previous = 0;
        double currentTime = -1.0e30, previousTime = -1.0e30;
        int size = 0;
        void destroy()
        {
            GLuint t[2] = {current, previous};
            glDeleteTextures(2, t);
            current = previous = 0;
        }
    };

    struct GpuLevel
    {
        surface::TerrainLevelKey key;
        int cells = 0;
        glm::dvec3 anchorBody {0.0};
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLsizei indexCount = 0;
        bool holeValid = false;
        surface::TerrainLevelKey holeKey;
        bool indicesBuilt = false;
        std::size_t bytes = 0;
        void destroy()
        {
            if (ebo) glDeleteBuffers(1, &ebo);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (vao) glDeleteVertexArrays(1, &vao);
            vao = vbo = ebo = 0;
        }
    };

    struct PendingLevel
    {
        surface::TerrainLevelKey key;
        std::shared_ptr<surface::TerrainLevel> level;
        std::shared_ptr<std::atomic<bool>> done;
        std::shared_ptr<std::atomic<bool>> cancel;
    };

    struct AtmosphereParams
    {
        bool present = false;
        double scaleHeightM = 8000.0;
        double shellM = 60000.0;
        double tauZenith = 0.0;
        glm::vec3 scatter {0.17f, 0.40f, 1.0f};
        glm::vec3 haze {0.6f, 0.75f, 1.0f};
        double hazeDensity0 = 0.0;   // per metre at the ground
        double cloudHeightM = 0.0;
        float cloudOpacity = 0.0f;
    };

    struct BodyRuntime
    {
        std::string id;
        std::shared_ptr<LoadResult> load;
        bool ready = false;
        bool failed = false;
        surface::BodySurfaceDescriptor descriptor;
        std::shared_ptr<surface::SurfaceField> field;
        std::shared_ptr<surface::CloudWeatherField> clouds;
        surface::SurfaceDetailPolicy policy;
        AtmosphereParams atmosphere;
        double contourKm = 1.0;
        GlobeTier globe;
        PendingGlobe pendingGlobe;
        CloudTier cloud;
        PendingCloud pendingCloud;
        std::map<int, GpuLevel> levels;          // displayed, by level index
        std::map<int, PendingLevel> pendingLevels;
        int terrainFace = -1;
        std::vector<PendingLevel> faceSwitch;    // a complete set on a new face
        GLuint ringTexture = 0;
        double ringInner = 0.0, ringOuter = 0.0;
        std::uint64_t lastUsedFrame = 0;
        glm::vec3 flatColour {0.5f};
    };

    struct AsteroidRuntime
    {
        surface::SmallBodyDescriptor descriptor;
        glm::dvec3 positionM {0.0};
        IndexedMesh mesh;
        int tier = 0;
        std::shared_ptr<std::atomic<bool>> done;
        std::shared_ptr<std::vector<AsteroidVertex>> vertices;
        std::shared_ptr<std::vector<std::uint32_t>> indices;
        int pendingTier = 0;
        glm::vec3 albedo {0.3f};
        std::uint64_t lastUsedFrame = 0;
    };

    struct ReservoirRuntime
    {
        std::shared_ptr<std::atomic<bool>> done;
        std::shared_ptr<surface::ReservoirSurfaceDescriptor> descriptor;
        bool ready = false;
    };

    glm::mat3 bodyToWorld(const FlightBody& b)
    {
        return glm::mat3(glm::vec3(b.prime), glm::vec3(b.north), glm::vec3(b.east));
    }
} // namespace

struct CelestialFlightRenderer::Impl
{
    bool initialized = false;
    CelestialRenderStyle style = CelestialRenderStyle::Modern;
    FlightRenderQuality quality;
    FlightRenderStats stats;
    JobPool pool;
    GpuTimers timers;
    std::uint64_t frameIndex = 0;
    std::atomic<long long> samples {0};

    Program globe, terrain, terrainDepth, cloud, atmosphere, ring, star, sprite, mesh;
    IndexedMesh sphereHigh, sphereLow;
    GLuint ringVao = 0, ringVbo = 0;
    GLuint spriteVao = 0, spriteVbo = 0;
    GLuint dummyCube = 0;

    std::unordered_map<std::string, BodyRuntime> bodies;
    std::unordered_map<std::string, AsteroidRuntime> asteroids;
    std::unordered_map<std::string, ReservoirRuntime> reservoirs;

    // Terrain of the last frame (near-pass depth).
    std::string terrainBodyId;
    glm::mat3 terrainBodyToWorld {1.0f};
    glm::dvec3 terrainBodyCentreRel {0.0};
    glm::dvec3 terrainCameraBody {0.0};

    glm::vec3 lineColour() const { return {0.45f, 1.0f, 0.55f}; }

    bool init(const FlightRenderQuality& q)
    {
        quality = q;
        using namespace shaders;
        bool ok = true;
        ok = ok && linkProgram(globe, {LogDepthVs, GlobeVs}, {LogDepthFs, SurfaceShading, GlobeFs});
        ok = ok && linkProgram(terrain, {LogDepthVs, TerrainVs}, {LogDepthFs, SurfaceShading, TerrainFs});
        ok = ok && linkProgram(terrainDepth, {TerrainDepthVs}, {TerrainDepthFs});
        ok = ok && linkProgram(cloud, {LogDepthVs, ShellVs}, {LogDepthFs, SurfaceShading, CloudFs});
        ok = ok && linkProgram(atmosphere, {LogDepthVs, ShellVs}, {LogDepthFs, AtmosphereFs});
        ok = ok && linkProgram(ring, {LogDepthVs, RingVs}, {LogDepthFs, SurfaceShading, RingFs});
        ok = ok && linkProgram(star, {LogDepthVs, ShellVs}, {LogDepthFs, SurfaceShading, StarFs});
        ok = ok && linkProgram(sprite, {LogDepthVs, SpriteVs}, {LogDepthFs, SpriteFs});
        ok = ok && linkProgram(mesh, {LogDepthVs, MeshVs}, {LogDepthFs, MeshFs});
        if (!ok)
            return false;
        sphereHigh = makeSphereMesh(64);
        sphereLow = makeSphereMesh(16);

        const float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
        glGenVertexArrays(1, &ringVao);
        glBindVertexArray(ringVao);
        glGenBuffers(1, &ringVbo);
        glBindBuffer(GL_ARRAY_BUFFER, ringVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);

        glGenVertexArrays(1, &spriteVao);
        glBindVertexArray(spriteVao);
        glGenBuffers(1, &spriteVbo);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                              reinterpret_cast<void*>(offsetof(SpriteVertex, colour)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                              reinterpret_cast<void*>(offsetof(SpriteVertex, size)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                              reinterpret_cast<void*>(offsetof(SpriteVertex, kind)));
        glBindVertexArray(0);

        const std::uint8_t black[4] = {0, 0, 0, 0};
        dummyCube = makeCube(1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, {black, black, black, black, black, black});

        timers.init();
        int threads = quality.workerThreads;
        if (threads <= 0)
            threads = std::clamp(static_cast<int>(std::thread::hardware_concurrency()) - 2, 2, 12);
        pool.start(threads);
        initialized = true;
        return true;
    }

    void destroy()
    {
        if (!initialized)
            return;
        pool.stop();
        for (auto& [id, b] : bodies)
        {
            b.globe.destroy();
            b.pendingGlobe.staging.destroy();
            b.cloud.destroy();
            for (auto& [k, l] : b.levels)
                l.destroy();
            if (b.ringTexture)
                glDeleteTextures(1, &b.ringTexture);
        }
        bodies.clear();
        for (auto& [id, a] : asteroids)
            a.mesh.destroy();
        asteroids.clear();
        sphereHigh.destroy();
        sphereLow.destroy();
        glDeleteBuffers(1, &ringVbo);
        glDeleteVertexArrays(1, &ringVao);
        glDeleteBuffers(1, &spriteVbo);
        glDeleteVertexArrays(1, &spriteVao);
        glDeleteTextures(1, &dummyCube);
        for (Program* p : {&globe, &terrain, &terrainDepth, &cloud, &atmosphere, &ring, &star, &sprite, &mesh})
            if (p->id)
                glDeleteProgram(p->id);
        timers.destroy();
        initialized = false;
    }

    // ---- loading --------------------------------------------------------
    static bool loadImage(const std::string& path, int channels, std::vector<std::uint8_t>& out, int& w, int& h)
    {
        stbi_set_flip_vertically_on_load_thread(0);
        int n = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, channels);
        if (!data)
            return false;
        out.assign(data, data + static_cast<std::size_t>(w) * h * channels);
        stbi_image_free(data);
        return true;
    }

    void startLoad(BodyRuntime& rt, const FlightBody& src)
    {
        auto result = std::make_shared<LoadResult>();
        rt.load = result;
        const FlightBody copy = src;
        pool.push(0, [result, copy] {
            std::string error;
            if (!surface::loadGeneratedBody(copy.descriptorPath, result->descriptor, &error))
            {
                result->error = error;
                result->done = true;
                return;
            }
            auto& d = result->descriptor;
            d.radiusM = copy.radiusM;
            if (!copy.importedAlbedoPath.empty())
            {
                d.source = surface::SurfaceSourceKind::ImportedRealData;
                auto maps = std::make_shared<surface::ImportedSurfaceMaps>();
                std::vector<std::uint8_t> rgb;
                int w = 0, h = 0;
                if (loadImage(copy.importedAlbedoPath, 3, rgb, w, h))
                {
                    maps->width = w;
                    maps->height = h;
                    maps->albedo.resize(static_cast<std::size_t>(w) * h);
                    for (std::size_t i = 0; i < maps->albedo.size(); ++i)
                        maps->albedo[i] = glm::pow(glm::vec3(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]) / 255.0f,
                                                   glm::vec3(2.2f));
                }
                std::vector<std::uint8_t> grey;
                int hw = 0, hh = 0;
                if (maps->width > 0 && !copy.importedHeightPath.empty() &&
                    loadImage(copy.importedHeightPath, 1, grey, hw, hh))
                {
                    // Height grey on the albedo grid (nearest).
                    const int w = maps->width, h = maps->height;
                    std::vector<std::uint8_t> g(maps->albedo.size());
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x)
                            g[static_cast<std::size_t>(y) * w + x] =
                                grey[static_cast<std::size_t>(y * hh / h) * hw + static_cast<std::size_t>(x * hw / w)];
                    if (copy.importedWaterFromHeight)
                    {
                        // Earth: the sea is the ocean colour of the albedo
                        // (the DEM has no zero sea). The curated maps do not
                        // share a longitude origin: the DEM is aligned by the
                        // column shift whose low half agrees best with the
                        // ocean.
                        const auto oceanColour = [](const glm::vec3& c) {
                            return c.b > 1.4f * c.r && c.b > 1.1f * c.g && (c.r + c.g + c.b) < 0.6f;
                        };
                        std::vector<std::uint8_t> sorted(g);
                        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
                        const std::uint8_t median = sorted[sorted.size() / 2];
                        int bestShift = 0;
                        long bestAgree = -1;
                        for (int shift : {0, w / 4, w / 2, 3 * w / 4})
                        {
                            long agree = 0;
                            for (int y = 0; y < h; y += 4)
                                for (int x = 0; x < w; x += 4)
                                {
                                    const std::size_t ai = static_cast<std::size_t>(y) * w + x;
                                    const std::size_t gi = static_cast<std::size_t>(y) * w + (x + shift) % w;
                                    agree += (g[gi] <= median) == oceanColour(maps->albedo[ai]) ? 1 : 0;
                                }
                            if (agree > bestAgree)
                            {
                                bestAgree = agree;
                                bestShift = shift;
                            }
                        }
                        std::vector<std::uint8_t> shifted(g.size());
                        for (int y = 0; y < h; ++y)
                            for (int x = 0; x < w; ++x)
                                shifted[static_cast<std::size_t>(y) * w + x] =
                                    g[static_cast<std::size_t>(y) * w + (x + bestShift) % w];
                        // Sea level grey: the low end of the land texels.
                        std::vector<std::uint8_t> land;
                        maps->water.assign(g.size(), 0);
                        for (std::size_t i = 0; i < g.size(); ++i)
                        {
                            maps->water[i] = oceanColour(maps->albedo[i]) ? 1 : 0;
                            if (!maps->water[i])
                                land.push_back(shifted[i]);
                        }
                        std::uint8_t sea = 0;
                        if (!land.empty())
                        {
                            std::nth_element(land.begin(), land.begin() + land.size() / 20, land.end());
                            sea = land[land.size() / 20];
                        }
                        std::fprintf(stderr, "[CelestialFlight] %s: curated sea %.2f of the surface, DEM shift %d, sea grey %d\n",
                                     copy.id.c_str(), 1.0 - static_cast<double>(land.size()) / g.size(), bestShift,
                                     static_cast<int>(sea));
                        maps->heightM.resize(g.size());
                        for (std::size_t i = 0; i < g.size(); ++i)
                            maps->heightM[i] = maps->water[i]
                                ? -3500.0f
                                : static_cast<float>(2.0 + std::max(0, shifted[i] - sea) / std::max(1.0, 255.0 - sea) *
                                                                copy.importedHeightRangeM);
                    }
                    else
                    {
                        surface::ImportedSurfaceMaps heights;
                        surface::importedHeightsFromGrey(g.data(), w, h, copy.importedHeightRangeM,
                                                         copy.importedHeightSigned, heights);
                        maps->heightM = std::move(heights.heightM);
                    }
                }
                if (maps->width > 0)
                    result->field = std::make_shared<surface::ImportedSurfaceField>(d, maps);
            }
            if (!result->field)
                result->field = std::shared_ptr<surface::SurfaceField>(surface::makeSurfaceField(d).release());
            // Gas giants: the bands ARE the clouds (no weather shell over them).
            if (copy.kind != FlightBodyKind::GasGiant && d.surfacePressurePa > 50.0 && d.cloudPotential > 0.0)
            {
                auto clouds = std::make_shared<surface::CloudWeatherField>(d);
                if (!clouds->layers().empty())
                    result->clouds = clouds;
            }
            result->ok = result->field != nullptr;
            result->done = true;
        });
    }

    static AtmosphereParams atmosphereFor(const surface::BodySurfaceDescriptor& d, FlightBodyKind kind,
                                          double radiusM, bool hasClouds,
                                          const surface::CloudWeatherField* clouds)
    {
        AtmosphereParams a;
        const double p = d.surfacePressurePa;
        const bool gas = kind == FlightBodyKind::GasGiant;
        if (!gas && p < 50.0)
            return a;
        a.present = true;
        a.scaleHeightM = d.atmosphereScaleHeightM > 0.0 ? d.atmosphereScaleHeightM : (gas ? 27000.0 : 8000.0);
        a.scaleHeightM = std::clamp(a.scaleHeightM, radiusM * 2.0e-4, radiusM * 0.02);
        a.shellM = a.scaleHeightM * 8.0;
        const double column = (p / 101325.0) * (9.81 / std::max(0.1, d.surfaceGravityMs2));
        const double co2 = fraction(d, "CO2"), ch4 = fraction(d, "CH4"), n2 = fraction(d, "N2");
        if (gas)
        {
            // A thin bluish limb; the band deck must stay readable through it.
            a.scatter = {0.45f, 0.62f, 1.0f};
            a.tauZenith = 0.3;
            a.scaleHeightM = std::max(a.scaleHeightM, radiusM * 0.0012);
            a.shellM = a.scaleHeightM * 8.0;
        }
        else if (co2 > 0.5 && p > 1.0e6)
        {
            a.scatter = {1.0f, 0.82f, 0.50f};
            a.tauZenith = 12.0;
        }
        else if (co2 > 0.5)
        {
            a.scatter = {1.0f, 0.62f, 0.34f}; // suspended dust
            a.tauZenith = std::max(0.35, 0.3 * column);
        }
        else if (ch4 > 0.01 && n2 > 0.5 && p > 1.0e4)
        {
            a.scatter = {1.0f, 0.58f, 0.22f}; // tholin haze
            a.tauZenith = std::min(8.0, 4.0 * column);
        }
        else
        {
            // Game-grade readability: a thick N2 atmosphere is hazy, not a
            // white wall (optical depth capped).
            a.scatter = {0.17f, 0.40f, 1.0f};
            a.tauZenith = std::clamp(0.3 * column, 0.02, 0.9);
        }
        const glm::vec3 n = a.scatter / std::max(a.scatter.r, std::max(a.scatter.g, a.scatter.b));
        a.haze = glm::mix(n, glm::vec3(1.0f), 0.35f) * 0.9f;
        a.hazeDensity0 = std::min(a.tauZenith, 1.5) / (a.scaleHeightM * 3.0);
        if (hasClouds && clouds && !clouds->layers().empty())
        {
            a.cloudHeightM = std::clamp(clouds->layers()[0].heightM, radiusM * 3.0e-4, a.shellM * 0.5);
            a.cloudOpacity = clouds->layers()[0].opacity;
        }
        return a;
    }

    void finishLoad(BodyRuntime& rt, const FlightBody& src)
    {
        LoadResult& r = *rt.load;
        if (!r.ok)
        {
            rt.failed = true;
            std::fprintf(stderr, "[CelestialFlight] %s: no surface (%s)\n", src.id.c_str(), r.error.c_str());
            return;
        }
        rt.descriptor = r.descriptor;
        rt.field = r.field;
        rt.clouds = r.clouds;
        rt.policy = surface::surfaceDetailPolicy(rt.descriptor);
        rt.atmosphere = atmosphereFor(rt.descriptor, src.kind, src.radiusM, rt.clouds != nullptr, rt.clouds.get());
        rt.contourKm = std::clamp(rt.descriptor.reliefAmplitudeKm / 6.0, 0.2, 3.0);
        rt.ready = true;
        rt.load.reset();
    }

    // ---- globe tiers ------------------------------------------------------
    void requestGlobe(BodyRuntime& rt, int size)
    {
        if (rt.pendingGlobe.faces)
            return;
        auto faces = std::make_shared<surface::GlobeFaces>();
        faces->size = size;
        auto done = std::make_shared<std::atomic<int>>(0);
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        rt.pendingGlobe = PendingGlobe {};
        rt.pendingGlobe.size = size;
        rt.pendingGlobe.faces = faces;
        rt.pendingGlobe.done = done;
        rt.pendingGlobe.cancel = cancel;
        const std::shared_ptr<surface::SurfaceField> field = rt.field;
        const int priority = 10 + static_cast<int>(std::log2(std::max(32, size)));
        for (int f = 0; f < 6; ++f)
            pool.push(priority, [this, field, faces, done, cancel, size, f] {
                if (surface::bakeGlobeFace(*field, size, f, *faces, cancel.get()))
                    samples += static_cast<long long>(size + 2) * (size + 2);
                ++*done;
            });
    }

    // Allocates a cube without data (filled face by face).
    static GLuint allocateCube(int size, GLenum internal, GLenum format, GLenum type)
    {
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_CUBE_MAP, t);
        for (int f = 0; f < 6; ++f)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, static_cast<GLint>(internal), size, size, 0, format,
                         type, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return t;
    }

    // A finished bake goes to the GPU in slices (two faces per frame), the
    // previous tier stays on screen until the new one is complete: no
    // frame pays for a whole 100 MB tier.
    void uploadGlobe(BodyRuntime& rt)
    {
        auto& p = rt.pendingGlobe;
        if (!p.faces || p.done->load() < 6)
            return;
        const double u0 = nowSeconds();
        if (p.cancel->load())
        {
            if (p.staging.albedo)
                p.staging.destroy();
            rt.pendingGlobe = PendingGlobe {};
            return;
        }
        const auto& f = *p.faces;
        if (!p.staging.albedo)
        {
            surface::finishGlobeBake(*p.faces);
            p.staging.albedo = allocateCube(f.size, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            p.staging.normal = allocateCube(f.size, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            p.staging.relief = allocateCube(f.size, GL_RG16F, GL_RG, GL_HALF_FLOAT);
            p.cursor = 0;
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int k = 0; k < 2 && p.cursor < 18; ++k, ++p.cursor)
        {
            const int texture = p.cursor / 6;
            const int face = p.cursor % 6;
            const auto fi = static_cast<std::size_t>(face);
            const GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face);
            if (texture == 0)
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, p.staging.albedo);
                glTexSubImage2D(target, 0, 0, 0, f.size, f.size, GL_RGBA, GL_UNSIGNED_BYTE, f.albedo[fi].data());
            }
            else if (texture == 1)
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, p.staging.normal);
                glTexSubImage2D(target, 0, 0, 0, f.size, f.size, GL_RGBA, GL_UNSIGNED_BYTE, f.normal[fi].data());
            }
            else
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, p.staging.relief);
                glTexSubImage2D(target, 0, 0, 0, f.size, f.size, GL_RG, GL_HALF_FLOAT, f.relief[fi].data());
            }
        }
        if (p.cursor >= 18)
        {
            for (GLuint t : {p.staging.albedo, p.staging.normal, p.staging.relief})
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, t);
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            }
            rt.globe.destroy();
            rt.globe = p.staging;
            rt.globe.size = f.size;
            rt.globe.bytes = f.bytes() * 4 / 3;
            rt.globe.meanAlbedo = f.meanAlbedo;
            rt.pendingGlobe = PendingGlobe {};
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        stats.cpuUploadMs += (nowSeconds() - u0) * 1000.0;
    }

    // ---- clouds -------------------------------------------------------------
    void updateClouds(BodyRuntime& rt, double worldTime)
    {
        if (!rt.clouds || !quality.clouds)
            return;
        auto& p = rt.pendingCloud;
        if (p.faces && p.done->load() >= 6)
        {
            const double u0 = nowSeconds();
            const auto& f = *p.faces;
            std::array<const void*, 6> c {};
            for (int i = 0; i < 6; ++i)
                c[static_cast<std::size_t>(i)] = f.coverage[static_cast<std::size_t>(i)].data();
            if (rt.cloud.previous)
                glDeleteTextures(1, &rt.cloud.previous);
            rt.cloud.previous = rt.cloud.current;
            rt.cloud.previousTime = rt.cloud.currentTime;
            rt.cloud.current = makeCube(f.size, GL_R8, GL_RED, GL_UNSIGNED_BYTE, c);
            rt.cloud.currentTime = p.timeSeconds;
            rt.cloud.size = f.size;
            p = PendingCloud {};
            stats.cpuUploadMs += (nowSeconds() - u0) * 1000.0;
        }
        // A new weather state every 20 minutes of world time, cross-faded.
        constexpr double Step = 1200.0;
        const double target = std::floor(worldTime / Step) * Step + Step;
        if (!p.faces && rt.cloud.currentTime < target - 0.5)
        {
            const double t = rt.cloud.current ? target : worldTime;
            auto faces = std::make_shared<surface::CloudFaces>();
            auto done = std::make_shared<std::atomic<int>>(0);
            p = PendingCloud {t, faces, done};
            const auto clouds = rt.clouds;
            const int size = quality.cloudFace;
            for (int f = 0; f < 6; ++f)
                pool.push(30, [clouds, faces, done, t, size, f, this] {
                    surface::bakeCloudFace(*clouds, 0, t, size, f, *faces);
                    samples += static_cast<long long>(size) * size;
                    ++*done;
                });
        }
    }

    // ---- local terrain ----------------------------------------------------
    void uploadLevel(GpuLevel& g, const surface::TerrainLevel& l)
    {
        g.key = l.key;
        g.cells = l.cells;
        g.anchorBody = l.anchorBody;
        glGenVertexArrays(1, &g.vao);
        glBindVertexArray(g.vao);
        glGenBuffers(1, &g.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(l.vertices.size() * sizeof(surface::TerrainVertex)),
                     l.vertices.data(), GL_STATIC_DRAW);
        const GLsizei stride = sizeof(surface::TerrainVertex);
        using V = surface::TerrainVertex;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(V, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(V, morphTarget)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(V, normal)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(V, lattice)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<void*>(offsetof(V, land)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<void*>(offsetof(V, water)));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(V, coastM)));
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(V, heightM)));
        glGenBuffers(1, &g.ebo);
        glBindVertexArray(0);
        g.indicesBuilt = false;
        g.bytes = l.vertices.size() * sizeof(surface::TerrainVertex);
    }

    void buildIndices(GpuLevel& g, const GpuLevel* finer)
    {
        int i0 = 0, i1 = 0, j0 = 0, j1 = 0;
        const bool hole = finer && surface::terrainHole(g.key, finer->key, g.cells, i0, i1, j0, j1);
        std::vector<std::uint32_t> idx;
        idx.reserve(static_cast<std::size_t>(g.cells) * g.cells * 6);
        const int n = g.cells;
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
            {
                if (hole && i >= i0 && i < i1 && j >= j0 && j < j1)
                    continue;
                const auto a = static_cast<std::uint32_t>(j * (n + 1) + i);
                const std::uint32_t b = a + 1;
                const std::uint32_t c = a + static_cast<std::uint32_t>(n + 1);
                const std::uint32_t d = c + 1;
                // Diagonal (0,0)-(1,1): the morph targets interpolate along it.
                idx.insert(idx.end(), {a, b, d, a, d, c});
            }
        glBindVertexArray(g.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * 4), idx.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(0);
        g.indexCount = static_cast<GLsizei>(idx.size());
        g.holeValid = hole;
        if (finer)
            g.holeKey = finer->key;
        g.indicesBuilt = true;
    }

    void scheduleLevel(BodyRuntime& rt, const surface::TerrainLevelKey& key, std::map<int, PendingLevel>& into,
                       int priority)
    {
        PendingLevel p;
        p.key = key;
        p.level = std::make_shared<surface::TerrainLevel>();
        p.done = std::make_shared<std::atomic<bool>>(false);
        p.cancel = std::make_shared<std::atomic<bool>>(false);
        const auto field = rt.field;
        const int cells = quality.terrainCells;
        auto level = p.level;
        auto done = p.done;
        auto cancel = p.cancel;
        pool.push(priority, [this, field, key, cells, level, done, cancel] {
            if (surface::buildTerrainLevel(*field, key, cells, *level, cancel.get()))
                samples += static_cast<long long>(level->samples);
            *done = true;
        });
        into[key.level] = p;
    }

    void dropTerrain(BodyRuntime& rt)
    {
        for (auto& [k, l] : rt.levels)
            l.destroy();
        rt.levels.clear();
        for (auto& [k, p] : rt.pendingLevels)
            *p.cancel = true;
        rt.pendingLevels.clear();
        for (auto& p : rt.faceSwitch)
            *p.cancel = true;
        rt.faceSwitch.clear();
        rt.terrainFace = -1;
    }

    void updateTerrain(BodyRuntime& rt, const glm::dvec3& cameraBody, double pixelAngle)
    {
        surface::LocalTerrainSettings s;
        s.cells = quality.terrainCells;
        s.pixelAngleRad = pixelAngle;
        s.nadirCellPixels = quality.terrainCellPixels;
        s.finestSpacingM = std::max(0.25, rt.policy.finestVertexSpacingM);
        s.maxLevels = quality.maxTerrainLevels;
        const glm::dvec3 nadir = glm::normalize(cameraBody);
        const double ground = rt.field->sample(nadir, 2000.0).heightM;
        const surface::LocalTerrainPlan plan =
            surface::planLocalTerrain(cameraBody, ground, rt.field->referenceRadiusM(), s, rt.terrainFace);
        if (!plan.active)
        {
            dropTerrain(rt);
            return;
        }
        // Finished jobs.
        for (auto it = rt.pendingLevels.begin(); it != rt.pendingLevels.end();)
        {
            if (!it->second.done->load())
            {
                ++it;
                continue;
            }
            if (!it->second.cancel->load() && it->second.key.face == rt.terrainFace)
            {
                auto old = rt.levels.find(it->first);
                if (old != rt.levels.end())
                    old->second.destroy();
                GpuLevel g;
                uploadLevel(g, *it->second.level);
                rt.levels[it->first] = g;
            }
            it = rt.pendingLevels.erase(it);
        }
        // Face switch: build the complete set on the new face, swap at once.
        if (plan.face != rt.terrainFace)
        {
            if (rt.terrainFace < 0 && rt.levels.empty())
            {
                rt.terrainFace = plan.face;
            }
            else
            {
                if (rt.faceSwitch.empty() || rt.faceSwitch.front().key.face != plan.face)
                {
                    for (auto& p : rt.faceSwitch)
                        *p.cancel = true;
                    rt.faceSwitch.clear();
                    std::map<int, PendingLevel> tmp;
                    for (const auto& key : plan.levels)
                        scheduleLevel(rt, key, tmp, 5 + key.level - plan.finestLevel);
                    for (auto& [k, p] : tmp)
                        rt.faceSwitch.push_back(p);
                }
                bool all = true;
                for (const auto& p : rt.faceSwitch)
                    all = all && p.done->load();
                if (all)
                {
                    for (auto& [k, l] : rt.levels)
                        l.destroy();
                    rt.levels.clear();
                    for (auto& [k, p] : rt.pendingLevels)
                        *p.cancel = true;
                    rt.pendingLevels.clear();
                    for (auto& p : rt.faceSwitch)
                    {
                        GpuLevel g;
                        uploadLevel(g, *p.level);
                        rt.levels[p.key.level] = g;
                    }
                    rt.faceSwitch.clear();
                    rt.terrainFace = plan.face;
                }
                return;
            }
        }
        // Levels outside the planned range go.
        for (auto it = rt.levels.begin(); it != rt.levels.end();)
        {
            if (it->first < plan.finestLevel || it->first > plan.coarsestLevel)
            {
                it->second.destroy();
                it = rt.levels.erase(it);
            }
            else
                ++it;
        }
        // Recentre with hysteresis (1/16 of a level). The morph window in
        // the vertex shader (0.70 .. 0.86 of the half extent) relies on it:
        // a finer level's footprint never reaches past 0.70 of the coarser
        // one, and every outer ring is beyond 0.86 - so the coarse hole
        // edge is unmorphed and the fine outer ring fully morphed.
        const std::int64_t threshold = quality.terrainCells / 16;
        for (const auto& key : plan.levels)
        {
            if (rt.pendingLevels.count(key.level))
                continue;
            auto it = rt.levels.find(key.level);
            const bool fresh = it == rt.levels.end();
            if (!fresh && std::llabs(it->second.key.ci - key.ci) <= threshold &&
                std::llabs(it->second.key.cj - key.cj) <= threshold)
                continue;
            scheduleLevel(rt, key, rt.pendingLevels, 5 + key.level - plan.finestLevel);
        }
    }

    void drawTerrain(BodyRuntime& rt, const FlightBody& b, const glm::mat3& toWorld, const glm::dvec3& centreRel,
                     const glm::dvec3& cameraBody, const glm::mat4& viewProj, double pixelAngle,
                     const glm::vec3& sunDir, const glm::vec3& sunColour, float hazeDensity)
    {
        if (rt.levels.empty())
            return;
        ScopedTimer timer(timers, TTerrain);
        // Holes follow the displayed finer level.
        const GpuLevel* finer = nullptr;
        for (auto& [k, g] : rt.levels)
        {
            const GpuLevel* f = finer && finer->key.level + 1 == k ? finer : nullptr;
            if (!g.indicesBuilt || (f ? !(g.holeValid && g.holeKey == f->key) : g.holeValid))
                buildIndices(g, f);
            finer = &g;
        }
        terrain.use();
        setCommon(terrain, sunDir, sunColour);
        terrain.set("uViewProj", viewProj);
        terrain.set("uBodyToWorld", toWorld);
        terrain.set("uBodyCentre", glm::vec3(centreRel));
        terrain.set("uPixelAngle", static_cast<float>(pixelAngle));
        terrain.set("uHazeColour", rt.atmosphere.haze);
        terrain.set("uHazeDensity", hazeDensity);
        terrain.set("uContourKm", static_cast<float>(rt.contourKm));
        terrain.set("uDetailStrength", quality.detailNormals ? 0.35f : 0.0f);
        const double finest = surface::terrain_chart::spacingM(rt.levels.begin()->first, b.radiusM);
        const double micro = std::max(0.5, finest * 0.7);
        terrain.set("uMicroScale", static_cast<float>(1.0 / micro));
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glDisable(GL_CULL_FACE);
        for (auto& [k, g] : rt.levels)
        {
            const glm::dvec3 anchorRel = glm::dmat3(toWorld) * g.anchorBody + centreRel;
            terrain.set("uAnchor", glm::vec3(anchorRel));
            const double du = surface::terrain_chart::step(g.key.level);
            double u = 0.0, v = 0.0;
            surface::terrain_chart::coordinates(g.key.face, glm::normalize(cameraBody), u, v);
            terrain.set("uCamLattice", glm::vec2(static_cast<float>(u / du - static_cast<double>(g.key.ci)),
                                                 static_cast<float>(v / du - static_cast<double>(g.key.cj))));
            terrain.set("uHalfCells", static_cast<float>(g.cells / 2));
            terrain.set("uLatticeBase", glm::vec2(static_cast<float>(((g.key.ci % 64) + 64) % 64),
                                                  static_cast<float>(((g.key.cj % 64) + 64) % 64)));
            const glm::dvec3 a = glm::mod(g.anchorBody, glm::dvec3(256.0 * micro));
            terrain.set("uAnchorBody", glm::vec3(a));
            glBindVertexArray(g.vao);
            glDrawElements(GL_TRIANGLES, g.indexCount, GL_UNSIGNED_INT, nullptr);
            stats.terrainTriangles += g.indexCount / 3;
            ++stats.terrainLevels;
        }
        glBindVertexArray(0);
        glDisable(GL_STENCIL_TEST);
    }

    void setCommon(const Program& p, const glm::vec3& sunDir, const glm::vec3& sunColour)
    {
        p.set("uLogC", static_cast<float>(2.0 / std::log2(LogFarM + 1.0)));
        p.set("uStyle", static_cast<int>(style));
        p.set("uLineColour", lineColour());
        p.set("uSunDir", sunDir);
        p.set("uSunColour", sunColour);
    }

    // ---- rings ----------------------------------------------------------------
    void ensureRingTexture(BodyRuntime& rt, const FlightBody& b)
    {
        if (rt.ringTexture || b.rings.empty())
            return;
        double inner = 1.0e300, outer = 0.0;
        for (const auto& r : b.rings)
        {
            inner = std::min(inner, r.innerM);
            outer = std::max(outer, r.outerM);
        }
        if (!(outer > inner))
            return;
        constexpr int N = 1024;
        std::vector<std::uint8_t> px(N * 4, 0);
        for (int i = 0; i < N; ++i)
        {
            const double r = inner + (outer - inner) * (i + 0.5) / N;
            glm::vec3 c(0.0f);
            float a = 0.0f;
            for (const auto& ring : b.rings)
                if (r >= ring.innerM && r <= ring.outerM)
                {
                    // Fine radial structure inside a band (deterministic from r).
                    const double w = std::sin(r * 1.7e-6) * 0.5 + std::sin(r * 7.3e-6 + 1.3) * 0.3 +
                                     std::sin(r * 2.9e-5 + 0.7) * 0.2;
                    const float op = std::clamp(ring.opacity * static_cast<float>(0.8 + 0.25 * w), 0.0f, 1.0f);
                    c = glm::mix(c, ring.tint, op);
                    a = 1.0f - (1.0f - a) * (1.0f - op);
                }
            px[static_cast<std::size_t>(i) * 4 + 0] = static_cast<std::uint8_t>(std::clamp(c.r, 0.0f, 1.0f) * 255);
            px[static_cast<std::size_t>(i) * 4 + 1] = static_cast<std::uint8_t>(std::clamp(c.g, 0.0f, 1.0f) * 255);
            px[static_cast<std::size_t>(i) * 4 + 2] = static_cast<std::uint8_t>(std::clamp(c.b, 0.0f, 1.0f) * 255);
            px[static_cast<std::size_t>(i) * 4 + 3] = static_cast<std::uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255);
        }
        glGenTextures(1, &rt.ringTexture);
        glBindTexture(GL_TEXTURE_2D, rt.ringTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        rt.ringInner = inner;
        rt.ringOuter = outer;
    }

    void drawRings(BodyRuntime& rt, const FlightBody& b, const glm::dvec3& rel, const glm::mat4& viewProj,
                   const glm::vec3& sunDir, const glm::vec3& sunColour)
    {
        ensureRingTexture(rt, b);
        if (!rt.ringTexture)
            return;
        ScopedTimer timer(timers, TRings);
        ring.use();
        setCommon(ring, sunDir, sunColour);
        ring.set("uViewProj", viewProj);
        ring.set("uCentre", glm::vec3(rel));
        ring.set("uAxisU", glm::vec3(glm::normalize(b.prime)));
        ring.set("uAxisV", glm::vec3(glm::normalize(b.east)));
        ring.set("uNormal", glm::vec3(glm::normalize(b.north)));
        ring.set("uOuter", static_cast<float>(rt.ringOuter));
        ring.set("uInner", static_cast<float>(rt.ringInner));
        ring.set("uPlanetRadius", static_cast<float>(b.radiusM));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rt.ringTexture);
        ring.set("uProfile", 0);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(ringVao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
    }

    // ---- asteroids --------------------------------------------------------------
    void updateAsteroids(const FlightCelestialFrame& frame, const glm::mat4& viewProj, double pixelAngle,
                         const glm::vec3& sunColour, std::vector<SpriteVertex>& sprites)
    {
        if (!quality.asteroids)
            return;
        for (const auto& resIn : frame.reservoirs)
        {
            auto& rr = reservoirs[resIn.id];
            if (!rr.done && !resIn.descriptorPath.empty())
            {
                rr.done = std::make_shared<std::atomic<bool>>(false);
                rr.descriptor = std::make_shared<surface::ReservoirSurfaceDescriptor>();
                auto d = rr.descriptor;
                auto done = rr.done;
                const std::string path = resIn.descriptorPath;
                pool.push(1, [d, done, path] {
                    surface::loadGeneratedReservoir(path, *d, nullptr);
                    *done = true;
                });
            }
            if (!rr.done || !rr.done->load())
                continue;
            // Annulus from the reservoir descriptor unless the frame gives it.
            FlightReservoir res = resIn;
            if (res.outerM <= 0.0)
            {
                res.innerM = rr.descriptor->innerAu * MetresPerAu;
                res.outerM = rr.descriptor->outerAu * MetresPerAu;
                res.halfThicknessM = 0.08 * 0.5 * (res.innerM + res.outerM);
            }
            const glm::dvec3 local = frame.cameraPositionM - res.centreM;
            const double r = std::sqrt(local.x * local.x + local.z * local.z);
            const double margin = surface::AsteroidCellSizeM * 3.0;
            if (r < res.innerM - margin || r > res.outerM + margin || std::abs(local.y) > res.halfThicknessM + margin)
                continue;
            const glm::ivec3 c0(glm::floor(local / surface::AsteroidCellSizeM));
            for (int dz = -2; dz <= 2; ++dz)
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                    {
                        const glm::ivec3 cell = c0 + glm::ivec3(dx, dy, dz);
                        const int count = surface::asteroidCandidatesInCell(*rr.descriptor, cell);
                        for (int k = 0; k < count; ++k)
                        {
                            const surface::SmallBodyDescriptor d =
                                surface::asteroidInReservoir(*rr.descriptor, cell, k);
                            const glm::dvec3 p = (glm::dvec3(cell) + surface::asteroidPositionInCell(d)) *
                                                 surface::AsteroidCellSizeM;
                            const double pr = std::sqrt(p.x * p.x + p.z * p.z);
                            if (pr < res.innerM || pr > res.outerM || std::abs(p.y) > res.halfThicknessM)
                                continue;
                            auto& a = asteroids[d.id];
                            a.descriptor = d;
                            a.positionM = res.centreM + p;
                            a.lastUsedFrame = frameIndex;
                            const glm::dvec3 rel = a.positionM - frame.cameraPositionM;
                            const double dist = glm::length(rel);
                            const double px = d.meanRadiusM / std::max(1.0, dist) / pixelAngle;
                            const int tier = px < 2.0 ? 0 : px < 20.0 ? 12 : px < 120.0 ? 24 : 48;
                            if (tier > a.tier && a.pendingTier == 0)
                                requestAsteroidMesh(a, tier);
                            if (a.mesh.count == 0 || px < 2.0)
                            {
                                const float lum = static_cast<float>(
                                    std::clamp(std::log10(std::max(1.0e-30, 0.1 * px * px)) / 3.0 + 1.0, 0.0, 1.0));
                                if (lum > 0.02)
                                {
                                    sprites.push_back(SpriteVertex {glm::vec3(rel), glm::vec4(sunColour * 0.8f, lum),
                                                                    2.0f, 0.0f});
                                    ++stats.asteroidSprites;
                                }
                            }
                        }
                    }
        }
        // Finished meshes; evict long unused ones.
        for (auto it = asteroids.begin(); it != asteroids.end();)
        {
            AsteroidRuntime& a = it->second;
            if (a.pendingTier && a.done && a.done->load())
            {
                a.mesh.destroy();
                IndexedMesh& m = a.mesh;
                glGenVertexArrays(1, &m.vao);
                glBindVertexArray(m.vao);
                glGenBuffers(1, &m.vbo);
                glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(a.vertices->size() * sizeof(AsteroidVertex)),
                             a.vertices->data(), GL_STATIC_DRAW);
                for (int i = 0; i < 3; ++i)
                {
                    glEnableVertexAttribArray(static_cast<GLuint>(i));
                    glVertexAttribPointer(static_cast<GLuint>(i), 3, GL_FLOAT, GL_FALSE, sizeof(AsteroidVertex),
                                          reinterpret_cast<void*>(static_cast<std::size_t>(i) * 12));
                }
                glGenBuffers(1, &m.ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(a.indices->size() * 4),
                             a.indices->data(), GL_STATIC_DRAW);
                glBindVertexArray(0);
                m.count = static_cast<GLsizei>(a.indices->size());
                m.bytes = a.vertices->size() * sizeof(AsteroidVertex) + a.indices->size() * 4;
                a.tier = a.pendingTier;
                a.pendingTier = 0;
                a.vertices.reset();
                a.indices.reset();
            }
            if (frameIndex - a.lastUsedFrame > 600 && a.pendingTier == 0)
            {
                a.mesh.destroy();
                it = asteroids.erase(it);
            }
            else
                ++it;
        }
        (void)viewProj;
    }

    void requestAsteroidMesh(AsteroidRuntime& a, int tier)
    {
        a.pendingTier = tier;
        a.done = std::make_shared<std::atomic<bool>>(false);
        a.vertices = std::make_shared<std::vector<AsteroidVertex>>();
        a.indices = std::make_shared<std::vector<std::uint32_t>>();
        auto done = a.done;
        auto verts = a.vertices;
        auto idx = a.indices;
        const surface::SmallBodyDescriptor d = a.descriptor;
        pool.push(20, [this, d, tier, done, verts, idx] {
            const surface::SmallBodyField field(d);
            const int n = tier;
            const double band = 2.0 * field.referenceRadiusM() * 1.57 / n;
            std::vector<glm::dvec3> pos;
            for (int f = 0; f < 6; ++f)
                for (int j = 0; j <= n; ++j)
                    for (int i = 0; i <= n; ++i)
                    {
                        const glm::dvec3 dir = surface::cubeMapDirection(
                            f, std::tan((2.0 * i / n - 1.0) * 0.785398), std::tan((2.0 * j / n - 1.0) * 0.785398));
                        const surface::SurfaceSample s = field.sample(dir, band);
                        pos.push_back(dir * (field.referenceRadiusM() + s.heightM));
                        const surface::SurfaceAppearance ap = field.appearance(s, dir, 0.1);
                        verts->push_back(AsteroidVertex {glm::vec3(pos.back()), glm::vec3(dir), ap.albedo});
                    }
            samples += static_cast<long long>(pos.size());
            for (int f = 0; f < 6; ++f)
            {
                const auto base = static_cast<std::uint32_t>(f * (n + 1) * (n + 1));
                for (int j = 0; j < n; ++j)
                    for (int i = 0; i < n; ++i)
                    {
                        const std::uint32_t a0 = base + static_cast<std::uint32_t>(j * (n + 1) + i);
                        idx->insert(idx->end(), {a0, a0 + static_cast<std::uint32_t>(n + 1), a0 + 1, a0 + 1,
                                                 a0 + static_cast<std::uint32_t>(n + 1),
                                                 a0 + static_cast<std::uint32_t>(n + 2)});
                    }
            }
            // Face normals averaged per vertex.
            std::vector<glm::dvec3> nrm(pos.size(), glm::dvec3(0.0));
            for (std::size_t t = 0; t + 2 < idx->size(); t += 3)
            {
                const auto i0 = (*idx)[t], i1 = (*idx)[t + 1], i2 = (*idx)[t + 2];
                const glm::dvec3 fn = glm::cross(pos[i1] - pos[i0], pos[i2] - pos[i0]);
                nrm[i0] += fn;
                nrm[i1] += fn;
                nrm[i2] += fn;
            }
            for (std::size_t i = 0; i < pos.size(); ++i)
            {
                glm::dvec3 nn = glm::length(nrm[i]) > 0.0 ? glm::normalize(nrm[i]) : glm::normalize(pos[i]);
                if (glm::dot(nn, pos[i]) < 0.0)
                    nn = -nn;
                (*verts)[i].normal = glm::vec3(nn);
            }
            *done = true;
        });
    }

    void drawAsteroids(const FlightCelestialFrame& frame, const glm::mat4& viewProj, const glm::vec3& sunDir,
                       const glm::vec3& sunColour)
    {
        bool any = false;
        for (const auto& [id, a] : asteroids)
            any = any || (a.mesh.count > 0 && a.lastUsedFrame == frameIndex);
        if (!any)
            return;
        ScopedTimer timer(timers, TAsteroids);
        mesh.use();
        setCommon(mesh, sunDir, sunColour);
        mesh.set("uViewProj", viewProj);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        for (const auto& [id, a] : asteroids)
        {
            if (a.mesh.count == 0 || a.lastUsedFrame != frameIndex)
                continue;
            const double period = surface::asteroidSpinPeriodSeconds(a.descriptor);
            const double angle = std::fmod(frame.worldTimeSeconds / period, 1.0) * 6.283185307179586;
            const glm::mat3 rot(glm::rotate(glm::mat4(1.0f), static_cast<float>(angle),
                                            glm::vec3(surface::asteroidSpinAxis(a.descriptor))));
            mesh.set("uRot", rot);
            mesh.set("uCentre", glm::vec3(a.positionM - frame.cameraPositionM));
            glBindVertexArray(a.mesh.vao);
            glDisable(GL_CULL_FACE);
            if (style == CelestialRenderStyle::EliteStyle)
            {
                mesh.set("uLinePass", 0.0f);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.0f, 1.0f);
                glDrawElements(GL_TRIANGLES, a.mesh.count, GL_UNSIGNED_INT, nullptr);
                glDisable(GL_POLYGON_OFFSET_FILL);
                mesh.set("uLinePass", 1.0f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDrawElements(GL_TRIANGLES, a.mesh.count, GL_UNSIGNED_INT, nullptr);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            else
                glDrawElements(GL_TRIANGLES, a.mesh.count, GL_UNSIGNED_INT, nullptr);
            ++stats.asteroidMeshes;
        }
        glBindVertexArray(0);
    }

    // ---- memory budget --------------------------------------------------------
    void enforceBudget()
    {
        std::size_t total = 0;
        for (const auto& [id, b] : bodies)
            total += b.globe.bytes;
        if (total <= quality.textureBudgetBytes)
            return;
        // Evict the least recently used globe tiers first (never this frame's).
        std::vector<BodyRuntime*> order;
        for (auto& [id, b] : bodies)
            if (b.globe.bytes > 0 && b.lastUsedFrame != frameIndex)
                order.push_back(&b);
        std::sort(order.begin(), order.end(),
                  [](const BodyRuntime* a, const BodyRuntime* b) { return a->lastUsedFrame < b->lastUsedFrame; });
        for (BodyRuntime* b : order)
        {
            if (total <= quality.textureBudgetBytes)
                break;
            total -= b->globe.bytes;
            b->globe.destroy();
            b->cloud.destroy();
        }
    }

    void render(const FlightCelestialFrame& frame, const glm::mat4& viewRotation, float fovY, int width, int height);
};

void CelestialFlightRenderer::Impl::render(const FlightCelestialFrame& frame, const glm::mat4& viewRotation,
                                           float fovY, int width, int height)
{
    const double t0 = nowSeconds();
    ++frameIndex;
    timers.beginFrame();
    const auto gpu = timers.result();
    stats = FlightRenderStats {};
    stats.gpuGlobesMs = gpu[TGlobes];
    stats.gpuTerrainMs = gpu[TTerrain];
    stats.gpuCloudsMs = gpu[TClouds];
    stats.gpuAtmosphereMs = gpu[TAtmosphere];
    stats.gpuRingsMs = gpu[TRings];
    stats.gpuStarsSpritesMs = gpu[TSprites];
    stats.gpuAsteroidsMs = gpu[TAsteroids];

    const double pixelAngle = fovY / std::max(1, height);
    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(1, height));
    const glm::mat4 proj = glm::perspective(fovY, aspect, 0.5f, 1.0e13f);
    glm::mat4 rot = viewRotation;
    rot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::mat4 viewProj = proj * rot;

    struct Item
    {
        const FlightBody* body;
        glm::dvec3 rel;
        double dist;
        double pxRadius;
    };
    std::vector<Item> items;
    items.reserve(frame.bodies.size());
    for (const auto& b : frame.bodies)
    {
        const glm::dvec3 rel = b.positionM - frame.cameraPositionM;
        const double d = std::max(1.0, glm::length(rel));
        const double ang = std::asin(std::min(1.0, b.radiusM / d));
        items.push_back(Item {&b, rel, d, ang / pixelAngle});
    }
    std::sort(items.begin(), items.end(),
              [](const Item& a, const Item& b) { return a.dist - a.body->radiusM > b.dist - b.body->radiusM; });

    const auto lightFor = [&](const FlightBody& body, glm::vec3& dir, glm::vec3& colour) {
        const FlightBody* best = nullptr;
        double bestFlux = -1.0;
        for (const auto& s : frame.bodies)
        {
            if (s.kind != FlightBodyKind::Star || &s == &body)
                continue;
            const double d2 = glm::dot(s.positionM - body.positionM, s.positionM - body.positionM) /
                              (MetresPerAu * MetresPerAu);
            const double flux = s.luminositySolar / std::max(1.0e-12, d2);
            if (flux > bestFlux)
            {
                bestFlux = flux;
                best = &s;
            }
        }
        if (!best)
        {
            dir = glm::normalize(glm::vec3(0.4f, 0.3f, 0.8f));
            colour = glm::vec3(1.0f);
            return;
        }
        dir = glm::vec3(glm::normalize(best->positionM - body.positionM));
        colour = blackbody(best->temperatureK) * static_cast<float>(std::clamp(std::pow(bestFlux, 0.2), 0.35, 1.6));
    };

    // Resource management (before drawing).
    std::string nearestId;
    double nearestAlt = std::numeric_limits<double>::infinity();
    double nearestRelative = std::numeric_limits<double>::infinity();
    for (const Item& it : items)
    {
        const FlightBody& b = *it.body;
        if (b.kind == FlightBodyKind::Star || b.descriptorPath.empty())
            continue;
        BodyRuntime& rt = bodies[b.id];
        rt.id = b.id;
        // "Used" = its disc is on screen; sprites need no textures, so a
        // body seen as a dot can lose its globe tier to the budget.
        if (it.pxRadius >= 1.5)
            rt.lastUsedFrame = frameIndex;
        if (!rt.ready && !rt.failed)
        {
            if (!rt.load && it.pxRadius > 1.0)
                startLoad(rt, b);
            if (rt.load && rt.load->done.load())
                finishLoad(rt, b);
        }
        if (!rt.ready)
            continue;
        // Local terrain goes to the solid body with the lowest altitude
        // relative to its radius.
        const double alt = it.dist - b.radiusM;
        if (b.kind == FlightBodyKind::Solid && alt / b.radiusM < nearestRelative)
        {
            nearestRelative = alt / b.radiusM;
            nearestAlt = alt;
            nearestId = b.id;
        }
        uploadGlobe(rt);
        if (it.pxRadius >= 1.5)
        {
            const int want = std::clamp(1 << static_cast<int>(std::ceil(std::log2(std::max(32.0, it.pxRadius)))), 32,
                                        quality.maxGlobeFace);
            if (want > rt.globe.size && !rt.pendingGlobe.faces)
                requestGlobe(rt, want);
            else if (rt.globe.size > 0 && want * 4 <= rt.globe.size && !rt.pendingGlobe.faces)
                requestGlobe(rt, want * 2);
            updateClouds(rt, frame.worldTimeSeconds);
        }
    }
    stats.nearestBody = nearestId;
    stats.nearestAltitudeM = std::isfinite(nearestAlt) ? nearestAlt : -1.0;

    // Terrain belongs to the nearest solid body within activation range.
    terrainBodyId.clear();
    for (auto& [id, rt] : bodies)
        if (id != nearestId && !rt.levels.empty())
            dropTerrain(rt);

    std::vector<SpriteVertex> sprites;
    stats.cpuPrepareMs = (nowSeconds() - t0) * 1000.0;
    const double t1 = nowSeconds();

    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (const Item& it : items)
    {
        const FlightBody& b = *it.body;
        glm::vec3 sunDir, sunColour;
        lightFor(b, sunDir, sunColour);
        const glm::mat3 toWorld = bodyToWorld(b);

        if (b.kind == FlightBodyKind::Star)
        {
            const glm::vec3 colour = blackbody(b.temperatureK);
            if (it.pxRadius >= 0.75)
            {
                ScopedTimer timer(timers, TGlobes);
                star.use();
                setCommon(star, sunDir, sunColour);
                star.set("uViewProj", viewProj);
                star.set("uBodyToWorld", toWorld);
                star.set("uCentre", glm::vec3(it.rel));
                star.set("uShellRadius", static_cast<float>(b.radiusM));
                star.set("uColour", colour);
                glDisable(GL_BLEND);
                glFrontFace(GL_CCW);
                const IndexedMesh& m = it.pxRadius > 40.0 ? sphereHigh : sphereLow;
                glBindVertexArray(m.vao);
                glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
                ++stats.globeBodies;
            }
            const double flux = b.luminositySolar / std::max(1.0e-12, it.dist * it.dist / (MetresPerAu * MetresPerAu));
            const float size = static_cast<float>(
                std::clamp(26.0 * std::pow(flux, 0.22) * height / 1080.0 + it.pxRadius * 3.0, 4.0, 900.0));
            const float intensity = static_cast<float>(std::clamp(0.35 + 0.25 * std::log10(1.0 + flux * 1.0e4), 0.2, 1.0));
            sprites.push_back(SpriteVertex {glm::vec3(it.rel), glm::vec4(colour, intensity), size, 1.0f});
            continue;
        }

        BodyRuntime* rt = nullptr;
        auto found = bodies.find(b.id);
        if (found != bodies.end())
            rt = &found->second;

        if (it.pxRadius < 1.5 || !rt || !rt->ready)
        {
            // Sub-pixel: reflected light only.
            const glm::vec3 albedo = rt && rt->globe.size ? rt->globe.meanAlbedo : glm::vec3(0.4f);
            const glm::vec3 toCam = glm::vec3(-glm::normalize(it.rel));
            const float phase = 0.5f + 0.5f * glm::dot(toCam, sunDir);
            const double flux = 0.3 * (b.radiusM / it.dist) * (b.radiusM / it.dist) * phase *
                                std::max(0.05f, glm::length(sunColour));
            const float intensity = static_cast<float>(std::clamp(std::log10(std::max(flux, 1.0e-30) / 1.0e-15) / 4.0,
                                                                  0.0, 1.0));
            if (intensity > 0.02)
            {
                sprites.push_back(SpriteVertex {glm::vec3(it.rel),
                                                glm::vec4(glm::mix(albedo * 1.6f, glm::vec3(1.0f), 0.4f) * sunColour,
                                                          intensity),
                                                1.5f + 2.5f * intensity, 0.0f});
                ++stats.spriteBodies;
            }
            if (it.pxRadius < 1.5)
                continue;
            if (!rt || !rt->ready)
                continue;
        }

        const AtmosphereParams& atm = rt->atmosphere;
        const double alt = it.dist - b.radiusM;
        float hazeDensity = 0.0f;
        if (atm.present && quality.atmosphere)
        {
            const double h = std::max(1.0, alt);
            const double factor = std::min(1.0, atm.scaleHeightM / h * (1.0 - std::exp(-h / atm.scaleHeightM)));
            hazeDensity = static_cast<float>(atm.hazeDensity0 * factor);
        }
        const glm::dvec3 cameraBody = glm::transpose(glm::dmat3(toWorld)) * (-it.rel);

        // Local terrain (nearest solid body only).
        if (b.id == nearestId && b.kind == FlightBodyKind::Solid && rt->field->solidSurface())
        {
            updateTerrain(*rt, cameraBody, pixelAngle);
            drawTerrain(*rt, b, toWorld, it.rel, cameraBody, viewProj, pixelAngle, sunDir, sunColour, hazeDensity);
            if (!rt->levels.empty())
            {
                terrainBodyId = b.id;
                terrainBodyToWorld = toWorld;
                terrainBodyCentreRel = it.rel;
                terrainCameraBody = cameraBody;
            }
        }

        // Globe (under the terrain: stencil).
        {
            ScopedTimer timer(timers, TGlobes);
            globe.use();
            setCommon(globe, sunDir, sunColour);
            globe.set("uViewProj", viewProj);
            globe.set("uBodyToWorld", toWorld);
            globe.set("uCentre", glm::vec3(it.rel));
            globe.set("uRadius", static_cast<float>(b.radiusM));
            const bool tex = rt->globe.size > 0;
            globe.set("uHasTexture", tex ? 1.0f : 0.0f);
            globe.set("uDisplace", tex && b.kind == FlightBodyKind::Solid ? 1.0f : 0.0f);
            globe.set("uIsGas", b.kind == FlightBodyKind::GasGiant ? 1.0f : 0.0f);
            globe.set("uFlat", rt->globe.size ? rt->globe.meanAlbedo : glm::vec3(0.35f));
            globe.set("uPixelAngle", static_cast<float>(pixelAngle));
            globe.set("uHazeColour", atm.haze);
            globe.set("uHazeDensity", hazeDensity);
            globe.set("uContourKm", static_cast<float>(rt->contourKm));
            const double texel = tex ? b.radiusM * 1.5708 / rt->globe.size : 0.0;
            globe.set("uMicroWavelength", quality.detailNormals && tex ? static_cast<float>(texel * 0.6) : 0.0f);
            globe.set("uDetailStrength", 0.25f);
            const bool cloudShadow = style == CelestialRenderStyle::Modern && rt->cloud.current;
            globe.set("uHasClouds", cloudShadow ? 1.0f : 0.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, tex ? rt->globe.albedo : dummyCube);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, tex ? rt->globe.normal : dummyCube);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_CUBE_MAP, tex ? rt->globe.relief : dummyCube);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cloudShadow ? rt->cloud.current : dummyCube);
            globe.set("uAlbedo", 0);
            globe.set("uNormal", 1);
            globe.set("uRelief", 2);
            globe.set("uCloudCover", 3);
            if (!rt->levels.empty())
            {
                glEnable(GL_STENCIL_TEST);
                glStencilFunc(GL_NOTEQUAL, 1, 0xff);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            }
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
            glDisable(GL_BLEND);
            const IndexedMesh& m = it.pxRadius > 60.0 ? sphereHigh : sphereLow;
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            glDisable(GL_STENCIL_TEST);
            ++stats.globeBodies;
        }

        if (!b.rings.empty() && quality.rings)
            drawRings(*rt, b, it.rel, viewProj, sunDir, sunColour);

        const bool inside = (it.dist < b.radiusM + atm.cloudHeightM);
        // Clouds.
        if (rt->cloud.current && quality.clouds && atm.cloudOpacity > 0.0f)
        {
            ScopedTimer timer(timers, TClouds);
            cloud.use();
            setCommon(cloud, sunDir, sunColour);
            cloud.set("uViewProj", viewProj);
            cloud.set("uBodyToWorld", toWorld);
            cloud.set("uCentre", glm::vec3(it.rel));
            cloud.set("uShellRadius", static_cast<float>(b.radiusM + atm.cloudHeightM));
            const double span = rt->cloud.currentTime - rt->cloud.previousTime;
            const float blend = rt->cloud.previous && span > 0.0
                ? static_cast<float>(std::clamp((frame.worldTimeSeconds - rt->cloud.previousTime) / span, 0.0, 1.0))
                : 1.0f;
            cloud.set("uBlend", blend);
            cloud.set("uOpacity", atm.cloudOpacity);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, rt->cloud.current);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, rt->cloud.previous ? rt->cloud.previous : rt->cloud.current);
            cloud.set("uCover", 0);
            cloud.set("uCoverPrev", 1);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glEnable(GL_CULL_FACE);
            glCullFace(inside ? GL_FRONT : GL_BACK);
            glFrontFace(GL_CCW);
            const IndexedMesh& m = it.pxRadius > 60.0 ? sphereHigh : sphereLow;
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
        }

        // Atmosphere (Elite: only the limb outline, seen from outside).
        const bool insideAtmosphere = it.dist < b.radiusM + atm.shellM;
        if (atm.present && quality.atmosphere &&
            !(style == CelestialRenderStyle::EliteStyle && insideAtmosphere))
        {
            ScopedTimer timer(timers, TAtmosphere);
            atmosphere.use();
            setCommon(atmosphere, sunDir, sunColour);
            atmosphere.set("uViewProj", viewProj);
            atmosphere.set("uBodyToWorld", toWorld);
            atmosphere.set("uCentre", glm::vec3(it.rel));
            const double shell = b.radiusM + atm.shellM;
            atmosphere.set("uShellRadius", static_cast<float>(shell));
            atmosphere.set("uPlanetRadius", static_cast<float>(b.radiusM));
            atmosphere.set("uScaleHeight", static_cast<float>(atm.scaleHeightM));
            atmosphere.set("uScatter", atm.scatter);
            atmosphere.set("uDensity", static_cast<float>(atm.tauZenith / (atm.scaleHeightM / b.radiusM)));
            atmosphere.set("uSteps", quality.atmosphereSteps);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            const bool insideShell = it.dist < shell;
            glEnable(GL_CULL_FACE);
            glCullFace(insideShell ? GL_FRONT : GL_BACK);
            glFrontFace(GL_CCW);
            const IndexedMesh& m = it.pxRadius > 60.0 ? sphereHigh : sphereLow;
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // Asteroid belts around the camera.
    {
        glm::vec3 sunDir(0.0f, 0.0f, 1.0f), sunColour(1.0f);
        FlightBody probe;
        probe.positionM = frame.cameraPositionM;
        lightFor(probe, sunDir, sunColour);
        updateAsteroids(frame, viewProj, pixelAngle, sunColour, sprites);
        drawAsteroids(frame, viewProj, sunDir, sunColour);
    }

    // Sprites last: depth tested against the bodies, additive.
    if (!sprites.empty())
    {
        ScopedTimer timer(timers, TSprites);
        sprite.use();
        setCommon(sprite, glm::vec3(0, 0, 1), glm::vec3(1));
        sprite.set("uViewProj", viewProj);
        glBindVertexArray(spriteVao);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sprites.size() * sizeof(SpriteVertex)), sprites.data(),
                     GL_STREAM_DRAW);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(sprites.size()));
        glDepthMask(GL_TRUE);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glBindVertexArray(0);
    }

    // Standard state for the caller.
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);

    enforceBudget();
    for (const auto& [id, b] : bodies)
    {
        stats.textureBytes += b.globe.bytes;
        if (b.cloud.current)
            stats.textureBytes += static_cast<std::size_t>(b.cloud.size) * b.cloud.size * 6 * (b.cloud.previous ? 2 : 1);
        for (const auto& [k, l] : b.levels)
            stats.meshBytes += l.bytes + static_cast<std::size_t>(l.indexCount) * 4;
    }
    for (const auto& [id, a] : asteroids)
        stats.meshBytes += a.mesh.bytes;
    stats.meshBytes += sphereHigh.bytes + sphereLow.bytes;
    stats.pendingJobs = pool.pending();
    for (const auto& [id, b] : bodies)
        stats.pendingJobs += b.pendingGlobe.faces ? 1 : 0;
    stats.workerBusySeconds = pool.busySeconds();
    stats.samplesTaken = samples.load();
    stats.cpuDrawMs = (nowSeconds() - t1) * 1000.0;
}

// ---- public -----------------------------------------------------------------
CelestialFlightRenderer::CelestialFlightRenderer() : m(std::make_unique<Impl>()) {}
CelestialFlightRenderer::~CelestialFlightRenderer()
{
    shutdown();
}

bool CelestialFlightRenderer::initialize(const FlightRenderQuality& quality)
{
    if (m->initialized)
        return true;
    return m->init(quality);
}

void CelestialFlightRenderer::shutdown()
{
    m->destroy();
}

bool CelestialFlightRenderer::initialized() const
{
    return m->initialized;
}

void CelestialFlightRenderer::setStyle(CelestialRenderStyle style)
{
    m->style = style;
}

CelestialRenderStyle CelestialFlightRenderer::style() const
{
    return m->style;
}

void CelestialFlightRenderer::setQuality(const FlightRenderQuality& quality)
{
    const int threads = m->quality.workerThreads;
    m->quality = quality;
    m->quality.workerThreads = threads;
}

const FlightRenderQuality& CelestialFlightRenderer::quality() const
{
    return m->quality;
}

void CelestialFlightRenderer::render(const FlightCelestialFrame& frame, const glm::mat4& viewRotation,
                                     float fovYRadians, int viewportWidth, int viewportHeight)
{
    if (!m->initialized)
        return;
    m->render(frame, viewRotation, fovYRadians, viewportWidth, viewportHeight);
}

void CelestialFlightRenderer::renderTerrainDepth(const glm::mat4& viewRotation, const glm::mat4& projection)
{
    if (!m->initialized || m->terrainBodyId.empty())
        return;
    auto it = m->bodies.find(m->terrainBodyId);
    if (it == m->bodies.end() || it->second.levels.empty())
        return;
    glm::mat4 rot = viewRotation;
    rot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::mat4 viewProj = projection * rot;
    Program& p = m->terrainDepth;
    p.use();
    p.set("uViewProj", viewProj);
    p.set("uBodyToWorld", m->terrainBodyToWorld);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    for (auto& [k, g] : it->second.levels)
    {
        const glm::dvec3 anchorRel = glm::dmat3(m->terrainBodyToWorld) * g.anchorBody + m->terrainBodyCentreRel;
        p.set("uAnchor", glm::vec3(anchorRel));
        const double du = world::celestial::surface::terrain_chart::step(g.key.level);
        double u = 0.0, v = 0.0;
        world::celestial::surface::terrain_chart::coordinates(g.key.face, glm::normalize(m->terrainCameraBody), u, v);
        p.set("uCamLattice", glm::vec2(static_cast<float>(u / du - static_cast<double>(g.key.ci)),
                                       static_cast<float>(v / du - static_cast<double>(g.key.cj))));
        p.set("uHalfCells", static_cast<float>(g.cells / 2));
        glBindVertexArray(g.vao);
        glDrawElements(GL_TRIANGLES, g.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(0);
}

bool CelestialFlightRenderer::waitUntilIdle(double timeoutSeconds)
{
    const double end = nowSeconds() + timeoutSeconds;
    while (nowSeconds() < end)
    {
        if (m->pool.pending() == 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return m->pool.pending() == 0;
}

const FlightRenderStats& CelestialFlightRenderer::stats() const
{
    return m->stats;
}

double CelestialFlightRenderer::groundHeightM(const std::string& bodyId, const glm::dvec3& dir) const
{
    auto it = m->bodies.find(bodyId);
    if (it == m->bodies.end() || !it->second.ready || !it->second.field)
        return std::numeric_limits<double>::quiet_NaN();
    return it->second.field->sample(glm::normalize(dir), 0.5).heightM;
}
std::shared_ptr<const surface::SurfaceField> CelestialFlightRenderer::surfaceField(const std::string& bodyId) const
{
    auto it = m->bodies.find(bodyId);
    if (it == m->bodies.end() || !it->second.ready)
        return nullptr;
    return it->second.field;
}
} // namespace render::celestial_flight
