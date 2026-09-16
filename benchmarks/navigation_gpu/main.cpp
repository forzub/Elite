#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kLocalSize = 256;
constexpr float kWorldHalfExtent = 12000.0f;
constexpr float kCellSize = 600.0f;
constexpr std::uint32_t kCellCapacity = 64;
constexpr float kInteractionMargin = 60.0f;
constexpr std::uint32_t kSeed = 0x51A7C0DEu;

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& a, float s)
{
    return {a.x * s, a.y * s, a.z * s};
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float lengthSquared(const Vec3& v)
{
    return dot(v, v);
}

float length(const Vec3& v)
{
    return std::sqrt(lengthSquared(v));
}

Vec3 normalized(const Vec3& v)
{
    const float len = length(v);
    if (len <= 1.0e-6f)
        return {1.0f, 0.0f, 0.0f};
    return v * (1.0f / len);
}

float distanceToSegment(const Vec3& p, const Vec3& a, const Vec3& b)
{
    const Vec3 ab = b - a;
    const float denom = lengthSquared(ab);
    if (denom <= 1.0e-8f)
        return length(p - a);

    const float t = std::clamp(dot(p - a, ab) / denom, 0.0f, 1.0f);
    return length(p - (a + ab * t));
}

struct alignas(16) GpuActor
{
    std::array<float, 4> positionRadius{};
    std::array<float, 4> velocity{};
    std::array<float, 4> acceleration{};
};

static_assert(sizeof(GpuActor) == 48, "std430 actor layout must remain 48 bytes");

struct CpuPrediction
{
    Vec3 center{};
    float sweepRadius = 0.0f;
    Vec3 endPosition{};
};

struct Scenario
{
    const char* name = "";
    float spawnHalfExtent = 0.0f;
    float maxSpeed = 0.0f;
    float maxAcceleration = 0.0f;
    float minRadius = 0.0f;
    float maxRadius = 0.0f;
    Vec3 corridorA{};
    Vec3 corridorB{};
    float corridorRadius = 0.0f;
};

struct Options
{
    int warmupIterations = 5;
    int measuredIterations = 30;
    float horizonSeconds = 3.0f;
    std::filesystem::path outputPath = "navigation_gpu_benchmark.csv";
};

struct GpuStats
{
    std::uint32_t overflowCount = 0;
    std::uint32_t outOfBoundsCount = 0;
    std::uint32_t occupiedCellCount = 0;
    std::uint32_t corridorActorCount = 0;
    std::uint32_t candidatePairCount = 0;
    std::uint32_t neighborCheckCount = 0;
    std::uint32_t reserved0 = 0;
    std::uint32_t reserved1 = 0;
};

static_assert(sizeof(GpuStats) == 32, "stats readback contract must remain 32 bytes");

struct ReferenceStats
{
    std::uint64_t candidatePairCount = 0;
    std::uint32_t corridorActorCount = 0;
};

struct IterationTiming
{
    double gpuBinMs = 0.0;
    double gpuNeighborMs = 0.0;
    double gpuTotalMs = 0.0;
    double cpuSubmitMs = 0.0;
};

struct ScenarioResult
{
    std::string scenario;
    std::size_t actorCount = 0;
    double gpuBinMedianMs = 0.0;
    double gpuNeighborMedianMs = 0.0;
    double gpuTotalMedianMs = 0.0;
    double gpuTotalP95Ms = 0.0;
    double cpuSubmitMedianMs = 0.0;
    GpuStats stats{};
    std::uint64_t referencePairs = 0;
    std::uint32_t referenceCorridor = 0;
    bool referenceAvailable = false;
    bool referenceOk = false;
    std::uint64_t memoryBytes = 0;
    std::uint64_t readbackBytes = sizeof(GpuStats);
    float maxSweepRadius = 0.0f;
};

void glfwErrorCallback(int code, const char* description)
{
    std::cerr << "[NavGpuBench][GLFW] code=" << code
              << " description=" << (description ? description : "<null>")
              << '\n';
}

GLuint compileShader(GLenum type, const char* source, const char* label)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE)
        return shader;

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(std::max(1, logLength)), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    glDeleteShader(shader);

    throw std::runtime_error(
        std::string("compute shader compile failed (") + label + "): " + log
    );
}

GLuint createComputeProgram(const char* source, const char* label)
{
    const GLuint shader = compileShader(GL_COMPUTE_SHADER, source, label);
    const GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE)
        return program;

    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(std::max(1, logLength)), '\0');
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    glDeleteProgram(program);

    throw std::runtime_error(
        std::string("compute program link failed (") + label + "): " + log
    );
}

GLuint createSsbo(std::size_t bytes, const void* data, GLenum usage)
{
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(bytes),
        data,
        usage
    );
    return buffer;
}

void clearUintBuffer(GLuint buffer)
{
    const std::uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glClearBufferData(
        GL_SHADER_STORAGE_BUFFER,
        GL_R32UI,
        GL_RED_INTEGER,
        GL_UNSIGNED_INT,
        &zero
    );
}

void bindSsbo(GLuint binding, GLuint buffer)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
}

void setUniform1ui(GLuint program, const char* name, std::uint32_t value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location < 0)
        throw std::runtime_error(std::string("missing uniform: ") + name);
    glUniform1ui(location, value);
}

void setUniform1f(GLuint program, const char* name, float value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location < 0)
        throw std::runtime_error(std::string("missing uniform: ") + name);
    glUniform1f(location, value);
}

void setUniform3f(GLuint program, const char* name, const Vec3& value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location < 0)
        throw std::runtime_error(std::string("missing uniform: ") + name);
    glUniform3f(location, value.x, value.y, value.z);
}

void setUniform3i(GLuint program, const char* name, int x, int y, int z)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location < 0)
        throw std::runtime_error(std::string("missing uniform: ") + name);
    glUniform3i(location, x, y, z);
}

const char* kPredictAndBinShader = R"GLSL(
#version 430 core
layout(local_size_x = 256) in;

struct Actor {
    vec4 positionRadius;
    vec4 velocity;
    vec4 acceleration;
};

struct PredictedActor {
    vec4 centerRadius;
    vec4 endPosition;
};

layout(std430, binding = 0) readonly buffer ActorBuffer {
    Actor actors[];
};

layout(std430, binding = 1) writeonly buffer PredictedBuffer {
    PredictedActor predicted[];
};

layout(std430, binding = 2) buffer CellCountBuffer {
    uint cellCounts[];
};

layout(std430, binding = 3) buffer CellSlotBuffer {
    uint cellSlots[];
};

layout(std430, binding = 4) buffer StatsBuffer {
    uvec4 stats0;
    uvec4 stats1;
};

uniform uint uActorCount;
uniform float uHorizon;
uniform vec3 uWorldHalfExtent;
uniform float uCellSize;
uniform ivec3 uGridDim;
uniform uint uCellCapacity;
uniform vec3 uCorridorA;
uniform vec3 uCorridorB;
uniform float uCorridorRadius;

bool validCell(ivec3 c)
{
    return all(greaterThanEqual(c, ivec3(0))) && all(lessThan(c, uGridDim));
}

uint flattenCell(ivec3 c)
{
    return uint((c.z * uGridDim.y + c.y) * uGridDim.x + c.x);
}

float distanceToSegment(vec3 p, vec3 a, vec3 b)
{
    vec3 ab = b - a;
    float denom = dot(ab, ab);
    if (denom <= 1.0e-8)
        return length(p - a);
    float t = clamp(dot(p - a, ab) / denom, 0.0, 1.0);
    return length(p - (a + ab * t));
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= uActorCount)
        return;

    Actor actor = actors[id];
    vec3 p0 = actor.positionRadius.xyz;
    float actorRadius = actor.positionRadius.w;
    vec3 velocity = actor.velocity.xyz;
    vec3 acceleration = actor.acceleration.xyz;

    vec3 p1 = p0 + velocity * uHorizon +
        0.5 * acceleration * uHorizon * uHorizon;

    // Conservative sphere around the current position. By the triangle
    // inequality it contains p(t) for every t in [0, horizon] under the
    // constant-acceleration predictor used by this prototype.
    float travelBound = length(velocity) * uHorizon +
        0.5 * length(acceleration) * uHorizon * uHorizon;
    float sweepRadius = actorRadius + travelBound;

    predicted[id].centerRadius = vec4(p0, sweepRadius);
    predicted[id].endPosition = vec4(p1, 0.0);

    if (distanceToSegment(p0, uCorridorA, uCorridorB) <=
        sweepRadius + uCorridorRadius)
    {
        atomicAdd(stats0.w, 1u); // corridorActorCount
    }

    ivec3 cell = ivec3(floor((p0 + uWorldHalfExtent) / uCellSize));
    if (!validCell(cell))
    {
        atomicAdd(stats0.y, 1u); // outOfBoundsCount
        return;
    }

    uint flat = flattenCell(cell);
    uint slot = atomicAdd(cellCounts[flat], 1u);
    if (slot == 0u)
        atomicAdd(stats0.z, 1u); // occupiedCellCount

    if (slot < uCellCapacity)
    {
        cellSlots[flat * uCellCapacity + slot] = id;
    }
    else
    {
        atomicAdd(stats0.x, 1u); // overflowCount
    }
}
)GLSL";

const char* kNeighborShader = R"GLSL(
#version 430 core
layout(local_size_x = 256) in;

struct PredictedActor {
    vec4 centerRadius;
    vec4 endPosition;
};

layout(std430, binding = 1) readonly buffer PredictedBuffer {
    PredictedActor predicted[];
};

layout(std430, binding = 2) readonly buffer CellCountBuffer {
    uint cellCounts[];
};

layout(std430, binding = 3) readonly buffer CellSlotBuffer {
    uint cellSlots[];
};

layout(std430, binding = 4) buffer StatsBuffer {
    uvec4 stats0;
    uvec4 stats1;
};

uniform uint uActorCount;
uniform vec3 uWorldHalfExtent;
uniform float uCellSize;
uniform ivec3 uGridDim;
uniform uint uCellCapacity;
uniform float uMaxSweepRadius;
uniform float uInteractionMargin;

bool validCell(ivec3 c)
{
    return all(greaterThanEqual(c, ivec3(0))) && all(lessThan(c, uGridDim));
}

uint flattenCell(ivec3 c)
{
    return uint((c.z * uGridDim.y + c.y) * uGridDim.x + c.x);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= uActorCount)
        return;

    vec4 self = predicted[id].centerRadius;
    ivec3 home = ivec3(floor((self.xyz + uWorldHalfExtent) / uCellSize));
    if (!validCell(home))
        return;

    // +1 cell is deliberate: two points can lie at opposite edges of their
    // cells, so ceil(radius / cellSize) alone is not a conservative index
    // distance bound.
    int range = int(ceil(
        (self.w + uMaxSweepRadius + uInteractionMargin) / uCellSize
    )) + 1;

    ivec3 beginCell = max(home - ivec3(range), ivec3(0));
    ivec3 endCell = min(home + ivec3(range), uGridDim - ivec3(1));

    for (int z = beginCell.z; z <= endCell.z; ++z)
    {
        for (int y = beginCell.y; y <= endCell.y; ++y)
        {
            for (int x = beginCell.x; x <= endCell.x; ++x)
            {
                uint flat = flattenCell(ivec3(x, y, z));
                uint count = min(cellCounts[flat], uCellCapacity);
                uint base = flat * uCellCapacity;

                for (uint slot = 0u; slot < count; ++slot)
                {
                    uint otherId = cellSlots[base + slot];
                    if (otherId <= id || otherId >= uActorCount)
                        continue;

                    atomicAdd(stats1.y, 1u); // neighborCheckCount

                    vec4 other = predicted[otherId].centerRadius;
                    vec3 delta = other.xyz - self.xyz;
                    float radius = self.w + other.w + uInteractionMargin;
                    if (dot(delta, delta) <= radius * radius)
                        atomicAdd(stats1.x, 1u); // candidatePairCount
                }
            }
        }
    }
}
)GLSL";

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto requireValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + name);
            return argv[++i];
        };

        if (arg == "--warmup")
            options.warmupIterations = std::stoi(requireValue("--warmup"));
        else if (arg == "--iterations")
            options.measuredIterations = std::stoi(requireValue("--iterations"));
        else if (arg == "--horizon")
            options.horizonSeconds = std::stof(requireValue("--horizon"));
        else if (arg == "--output")
            options.outputPath = requireValue("--output");
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "navigation_gpu_benchmark [--warmup N] [--iterations N] "
                   "[--horizon seconds] [--output file.csv]\n";
            std::exit(0);
        }
        else
            throw std::runtime_error("unknown argument: " + arg);
    }

    if (options.warmupIterations < 0 || options.measuredIterations <= 0)
        throw std::runtime_error("iteration counts must be non-negative / positive");
    if (!(options.horizonSeconds > 0.0f))
        throw std::runtime_error("horizon must be positive");

    return options;
}

Vec3 randomDirection(std::mt19937& rng)
{
    std::uniform_real_distribution<float> component(-1.0f, 1.0f);
    for (;;)
    {
        Vec3 v{component(rng), component(rng), component(rng)};
        const float l2 = lengthSquared(v);
        if (l2 > 1.0e-5f && l2 <= 1.0f)
            return normalized(v);
    }
}

std::vector<GpuActor> generateActors(
    std::size_t count,
    const Scenario& scenario,
    std::uint32_t seed
)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> position(
        -scenario.spawnHalfExtent,
        scenario.spawnHalfExtent
    );
    std::uniform_real_distribution<float> speed(0.0f, scenario.maxSpeed);
    std::uniform_real_distribution<float> accel(0.0f, scenario.maxAcceleration);
    std::uniform_real_distribution<float> radius(
        scenario.minRadius,
        scenario.maxRadius
    );

    std::vector<GpuActor> actors(count);
    for (auto& actor : actors)
    {
        const Vec3 p{position(rng), position(rng), position(rng)};
        const Vec3 v = randomDirection(rng) * speed(rng);
        const Vec3 a = randomDirection(rng) * accel(rng);

        actor.positionRadius = {p.x, p.y, p.z, radius(rng)};
        actor.velocity = {v.x, v.y, v.z, 0.0f};
        actor.acceleration = {a.x, a.y, a.z, 0.0f};
    }
    return actors;
}

std::vector<CpuPrediction> predictCpu(
    const std::vector<GpuActor>& actors,
    float horizon,
    float& maxSweepRadius
)
{
    std::vector<CpuPrediction> result;
    result.reserve(actors.size());
    maxSweepRadius = 0.0f;

    for (const auto& actor : actors)
    {
        const Vec3 p{
            actor.positionRadius[0],
            actor.positionRadius[1],
            actor.positionRadius[2]
        };
        const Vec3 v{actor.velocity[0], actor.velocity[1], actor.velocity[2]};
        const Vec3 a{
            actor.acceleration[0],
            actor.acceleration[1],
            actor.acceleration[2]
        };

        const float travelBound = length(v) * horizon +
            0.5f * length(a) * horizon * horizon;
        const float sweepRadius = actor.positionRadius[3] + travelBound;
        const Vec3 end = p + v * horizon + a * (0.5f * horizon * horizon);

        result.push_back({p, sweepRadius, end});
        maxSweepRadius = std::max(maxSweepRadius, sweepRadius);
    }

    return result;
}

ReferenceStats computeReference(
    const std::vector<CpuPrediction>& predicted,
    const Scenario& scenario
)
{
    ReferenceStats result;

    for (const auto& actor : predicted)
    {
        if (distanceToSegment(actor.center, scenario.corridorA, scenario.corridorB) <=
            actor.sweepRadius + scenario.corridorRadius)
        {
            ++result.corridorActorCount;
        }
    }

    for (std::size_t i = 0; i < predicted.size(); ++i)
    {
        for (std::size_t j = i + 1; j < predicted.size(); ++j)
        {
            const Vec3 delta = predicted[j].center - predicted[i].center;
            const float radius = predicted[i].sweepRadius +
                predicted[j].sweepRadius + kInteractionMargin;
            if (lengthSquared(delta) <= radius * radius)
                ++result.candidatePairCount;
        }
    }

    return result;
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    const std::size_t index = static_cast<std::size_t>(std::ceil(
        clamped * static_cast<double>(values.size())
    )) - (clamped > 0.0 ? 1u : 0u);
    return values[std::min(index, values.size() - 1)];
}

void configurePredictProgram(
    GLuint program,
    std::size_t actorCount,
    const Scenario& scenario,
    const Options& options,
    int gridDim
)
{
    glUseProgram(program);
    setUniform1ui(program, "uActorCount", static_cast<std::uint32_t>(actorCount));
    setUniform1f(program, "uHorizon", options.horizonSeconds);
    setUniform3f(
        program,
        "uWorldHalfExtent",
        {kWorldHalfExtent, kWorldHalfExtent, kWorldHalfExtent}
    );
    setUniform1f(program, "uCellSize", kCellSize);
    setUniform3i(program, "uGridDim", gridDim, gridDim, gridDim);
    setUniform1ui(program, "uCellCapacity", kCellCapacity);
    setUniform3f(program, "uCorridorA", scenario.corridorA);
    setUniform3f(program, "uCorridorB", scenario.corridorB);
    setUniform1f(program, "uCorridorRadius", scenario.corridorRadius);
}

void configureNeighborProgram(
    GLuint program,
    std::size_t actorCount,
    float maxSweepRadius,
    int gridDim
)
{
    glUseProgram(program);
    setUniform1ui(program, "uActorCount", static_cast<std::uint32_t>(actorCount));
    setUniform3f(
        program,
        "uWorldHalfExtent",
        {kWorldHalfExtent, kWorldHalfExtent, kWorldHalfExtent}
    );
    setUniform1f(program, "uCellSize", kCellSize);
    setUniform3i(program, "uGridDim", gridDim, gridDim, gridDim);
    setUniform1ui(program, "uCellCapacity", kCellCapacity);
    setUniform1f(program, "uMaxSweepRadius", maxSweepRadius);
    setUniform1f(program, "uInteractionMargin", kInteractionMargin);
}

IterationTiming runIteration(
    GLuint predictProgram,
    GLuint neighborProgram,
    GLuint cellCountBuffer,
    GLuint statsBuffer,
    std::size_t actorCount,
    const std::array<GLuint, 3>& timestampQueries
)
{
    const GLuint groupCount = static_cast<GLuint>(
        (actorCount + kLocalSize - 1) / kLocalSize
    );

    const auto cpuBegin = std::chrono::steady_clock::now();

    glQueryCounter(timestampQueries[0], GL_TIMESTAMP);

    clearUintBuffer(cellCountBuffer);
    clearUintBuffer(statsBuffer);

    glUseProgram(predictProgram);
    glDispatchCompute(groupCount, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glQueryCounter(timestampQueries[1], GL_TIMESTAMP);

    glUseProgram(neighborProgram);
    glDispatchCompute(groupCount, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glQueryCounter(timestampQueries[2], GL_TIMESTAMP);

    const auto cpuEnd = std::chrono::steady_clock::now();

    std::array<GLuint64, 3> gpuNs{};
    for (std::size_t i = 0; i < timestampQueries.size(); ++i)
    {
        glGetQueryObjectui64v(
            timestampQueries[i],
            GL_QUERY_RESULT,
            &gpuNs[i]
        );
    }

    IterationTiming timing;
    timing.gpuBinMs = static_cast<double>(gpuNs[1] - gpuNs[0]) / 1.0e6;
    timing.gpuNeighborMs = static_cast<double>(gpuNs[2] - gpuNs[1]) / 1.0e6;
    timing.gpuTotalMs = static_cast<double>(gpuNs[2] - gpuNs[0]) / 1.0e6;
    timing.cpuSubmitMs = std::chrono::duration<double, std::milli>(
        cpuEnd - cpuBegin
    ).count();
    return timing;
}

ScenarioResult runScenario(
    GLuint predictProgram,
    GLuint neighborProgram,
    const Scenario& scenario,
    std::size_t actorCount,
    const Options& options
)
{
    const std::uint32_t scenarioSalt =
        scenario.name[0] == 'h' ? 0x48425542u : 0x43525549u;
    const auto actors = generateActors(
        actorCount,
        scenario,
        kSeed ^ scenarioSalt ^ static_cast<std::uint32_t>(actorCount)
    );

    float maxSweepRadius = 0.0f;
    const auto cpuPrediction = predictCpu(
        actors,
        options.horizonSeconds,
        maxSweepRadius
    );

    const int gridDim = static_cast<int>(std::ceil(
        (2.0f * kWorldHalfExtent) / kCellSize
    ));
    const std::size_t cellCount = static_cast<std::size_t>(gridDim) *
        static_cast<std::size_t>(gridDim) *
        static_cast<std::size_t>(gridDim);

    const std::size_t actorBytes = actors.size() * sizeof(GpuActor);
    const std::size_t predictedBytes = actors.size() * 32u;
    const std::size_t cellCountBytes = cellCount * sizeof(std::uint32_t);
    const std::size_t cellSlotBytes = cellCount *
        static_cast<std::size_t>(kCellCapacity) * sizeof(std::uint32_t);
    const std::size_t statsBytes = sizeof(GpuStats);

    const GLuint actorBuffer = createSsbo(
        actorBytes,
        actors.data(),
        GL_STATIC_DRAW
    );
    const GLuint predictedBuffer = createSsbo(
        predictedBytes,
        nullptr,
        GL_DYNAMIC_DRAW
    );
    const GLuint cellCountBuffer = createSsbo(
        cellCountBytes,
        nullptr,
        GL_DYNAMIC_DRAW
    );
    const GLuint cellSlotBuffer = createSsbo(
        cellSlotBytes,
        nullptr,
        GL_DYNAMIC_DRAW
    );
    const GLuint statsBuffer = createSsbo(
        statsBytes,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    bindSsbo(0, actorBuffer);
    bindSsbo(1, predictedBuffer);
    bindSsbo(2, cellCountBuffer);
    bindSsbo(3, cellSlotBuffer);
    bindSsbo(4, statsBuffer);

    configurePredictProgram(
        predictProgram,
        actorCount,
        scenario,
        options,
        gridDim
    );
    configureNeighborProgram(
        neighborProgram,
        actorCount,
        maxSweepRadius,
        gridDim
    );

    std::array<GLuint, 3> timestampQueries{};
    glGenQueries(
        static_cast<GLsizei>(timestampQueries.size()),
        timestampQueries.data()
    );

    glFinish();

    for (int i = 0; i < options.warmupIterations; ++i)
    {
        (void)runIteration(
            predictProgram,
            neighborProgram,
            cellCountBuffer,
            statsBuffer,
            actorCount,
            timestampQueries
        );
    }

    std::vector<double> gpuBin;
    std::vector<double> gpuNeighbor;
    std::vector<double> gpuTotal;
    std::vector<double> cpuSubmit;
    gpuBin.reserve(options.measuredIterations);
    gpuNeighbor.reserve(options.measuredIterations);
    gpuTotal.reserve(options.measuredIterations);
    cpuSubmit.reserve(options.measuredIterations);

    for (int i = 0; i < options.measuredIterations; ++i)
    {
        const IterationTiming timing = runIteration(
            predictProgram,
            neighborProgram,
            cellCountBuffer,
            statsBuffer,
            actorCount,
            timestampQueries
        );
        gpuBin.push_back(timing.gpuBinMs);
        gpuNeighbor.push_back(timing.gpuNeighborMs);
        gpuTotal.push_back(timing.gpuTotalMs);
        cpuSubmit.push_back(timing.cpuSubmitMs);
    }

    GpuStats stats{};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, statsBuffer);
    glGetBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        static_cast<GLsizeiptr>(sizeof(stats)),
        &stats
    );

    ScenarioResult result;
    result.scenario = scenario.name;
    result.actorCount = actorCount;
    result.gpuBinMedianMs = percentile(gpuBin, 0.50);
    result.gpuNeighborMedianMs = percentile(gpuNeighbor, 0.50);
    result.gpuTotalMedianMs = percentile(gpuTotal, 0.50);
    result.gpuTotalP95Ms = percentile(gpuTotal, 0.95);
    result.cpuSubmitMedianMs = percentile(cpuSubmit, 0.50);
    result.stats = stats;
    result.maxSweepRadius = maxSweepRadius;
    result.memoryBytes = static_cast<std::uint64_t>(
        actorBytes + predictedBytes + cellCountBytes + cellSlotBytes + statsBytes
    );

    if (actorCount <= 1000)
    {
        const ReferenceStats reference = computeReference(cpuPrediction, scenario);
        result.referenceAvailable = true;
        result.referencePairs = reference.candidatePairCount;
        result.referenceCorridor = reference.corridorActorCount;
        result.referenceOk =
            stats.overflowCount == 0 &&
            stats.outOfBoundsCount == 0 &&
            result.referencePairs == stats.candidatePairCount &&
            result.referenceCorridor == stats.corridorActorCount;
    }

    glDeleteQueries(
        static_cast<GLsizei>(timestampQueries.size()),
        timestampQueries.data()
    );

    const std::array<GLuint, 5> buffers{
        actorBuffer,
        predictedBuffer,
        cellCountBuffer,
        cellSlotBuffer,
        statsBuffer
    };
    glDeleteBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());

    return result;
}

void printResult(const ScenarioResult& result)
{
    const double memoryMiB = static_cast<double>(result.memoryBytes) /
        (1024.0 * 1024.0);
    const bool structurallyValid =
        result.stats.overflowCount == 0 && result.stats.outOfBoundsCount == 0;

    std::cout
        << std::fixed << std::setprecision(4)
        << "[NavGpuBench] scenario=" << result.scenario
        << " actors=" << result.actorCount
        << " gpu_bin_ms=" << result.gpuBinMedianMs
        << " gpu_neighbor_ms=" << result.gpuNeighborMedianMs
        << " gpu_total_ms=" << result.gpuTotalMedianMs
        << " gpu_p95_ms=" << result.gpuTotalP95Ms
        << " cpu_submit_ms=" << result.cpuSubmitMedianMs
        << " pairs=" << result.stats.candidatePairCount
        << " neighbor_checks=" << result.stats.neighborCheckCount
        << " corridor=" << result.stats.corridorActorCount
        << " occupied_cells=" << result.stats.occupiedCellCount
        << " overflow=" << result.stats.overflowCount
        << " out_of_bounds=" << result.stats.outOfBoundsCount
        << " max_sweep_radius_m=" << result.maxSweepRadius
        << " memory_mib=" << memoryMiB
        << " readback_bytes=" << result.readbackBytes
        << " valid=" << (structurallyValid ? 1 : 0);

    if (result.referenceAvailable)
    {
        std::cout
            << " reference_pairs=" << result.referencePairs
            << " reference_corridor=" << result.referenceCorridor
            << " reference_ok=" << (result.referenceOk ? 1 : 0);
    }

    std::cout << '\n';
}

void writeCsvHeader(std::ofstream& csv)
{
    csv
        << "scenario,actors,gpu_bin_median_ms,gpu_neighbor_median_ms,"
           "gpu_total_median_ms,gpu_total_p95_ms,cpu_submit_median_ms,"
           "candidate_pairs,neighbor_checks,corridor_actors,occupied_cells,"
           "overflow,out_of_bounds,max_sweep_radius_m,memory_bytes,"
           "readback_bytes,reference_available,reference_pairs,"
           "reference_corridor,reference_ok\n";
}

void writeCsvRow(std::ofstream& csv, const ScenarioResult& result)
{
    csv
        << result.scenario << ','
        << result.actorCount << ','
        << result.gpuBinMedianMs << ','
        << result.gpuNeighborMedianMs << ','
        << result.gpuTotalMedianMs << ','
        << result.gpuTotalP95Ms << ','
        << result.cpuSubmitMedianMs << ','
        << result.stats.candidatePairCount << ','
        << result.stats.neighborCheckCount << ','
        << result.stats.corridorActorCount << ','
        << result.stats.occupiedCellCount << ','
        << result.stats.overflowCount << ','
        << result.stats.outOfBoundsCount << ','
        << result.maxSweepRadius << ','
        << result.memoryBytes << ','
        << result.readbackBytes << ','
        << (result.referenceAvailable ? 1 : 0) << ','
        << result.referencePairs << ','
        << result.referenceCorridor << ','
        << (result.referenceOk ? 1 : 0)
        << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    GLFWwindow* window = nullptr;
    GLuint predictProgram = 0;
    GLuint neighborProgram = 0;

    try
    {
        const Options options = parseOptions(argc, argv);

        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit())
            throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(64, 64, "Elite Navigation GPU Benchmark", nullptr, nullptr);
        if (!window)
            throw std::runtime_error("failed to create hidden OpenGL 4.3 context");

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        const int gladVersion = gladLoadGL(glfwGetProcAddress);
        if (gladVersion == 0)
            throw std::runtime_error("gladLoadGL failed");

        GLint major = 0;
        GLint minor = 0;
        GLint maxInvocations = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);

        if (major < 4 || (major == 4 && minor < 3))
            throw std::runtime_error("OpenGL 4.3+ is required");
        if (maxInvocations < static_cast<GLint>(kLocalSize))
            throw std::runtime_error("GPU compute work-group limit is below 256");

        std::cout
            << "[NavGpuBench] OpenGL=" << major << '.' << minor
            << " vendor=\"" << reinterpret_cast<const char*>(glGetString(GL_VENDOR)) << '"'
            << " renderer=\"" << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << '"'
            << " glsl=\"" << reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)) << '"'
            << " horizon_s=" << options.horizonSeconds
            << " warmup=" << options.warmupIterations
            << " iterations=" << options.measuredIterations
            << '\n';

        predictProgram = createComputeProgram(
            kPredictAndBinShader,
            "predict-and-bin"
        );
        neighborProgram = createComputeProgram(
            kNeighborShader,
            "neighbor-query"
        );

        const std::array<Scenario, 2> scenarios{{
            {
                "cruise",
                9000.0f,
                250.0f,
                8.0f,
                4.0f,
                28.0f,
                {0.0f, 0.0f, 0.0f},
                {9000.0f, 0.0f, 0.0f},
                600.0f
            },
            {
                "hub",
                3500.0f,
                120.0f,
                6.0f,
                3.0f,
                24.0f,
                {0.0f, 0.0f, 0.0f},
                {5000.0f, 0.0f, 0.0f},
                350.0f
            }
        }};
        const std::array<std::size_t, 3> actorCounts{{1000, 5000, 10000}};

        const std::filesystem::path absoluteOutput =
            std::filesystem::absolute(options.outputPath);
        std::ofstream csv(absoluteOutput, std::ios::out | std::ios::trunc);
        if (!csv)
            throw std::runtime_error("failed to open CSV output: " + absoluteOutput.string());
        writeCsvHeader(csv);

        bool allReferenceChecksPassed = true;
        for (const auto& scenario : scenarios)
        {
            for (const std::size_t actorCount : actorCounts)
            {
                const ScenarioResult result = runScenario(
                    predictProgram,
                    neighborProgram,
                    scenario,
                    actorCount,
                    options
                );
                printResult(result);
                writeCsvRow(csv, result);
                csv.flush();

                if (result.referenceAvailable && !result.referenceOk)
                    allReferenceChecksPassed = false;
            }
        }

        std::cout
            << "[NavGpuBench] csv=\"" << absoluteOutput.string() << "\"\n"
            << "[NavGpuBench] reference="
            << (allReferenceChecksPassed ? "PASS" : "FAIL")
            << '\n';

        glDeleteProgram(neighborProgram);
        glDeleteProgram(predictProgram);
        neighborProgram = 0;
        predictProgram = 0;

        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();

        return allReferenceChecksPassed ? 0 : 2;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[NavGpuBench][FAIL] " << e.what() << '\n';

        if (neighborProgram)
            glDeleteProgram(neighborProgram);
        if (predictProgram)
            glDeleteProgram(predictProgram);
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
}
