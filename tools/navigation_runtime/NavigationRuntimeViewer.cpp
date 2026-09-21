#include "NavigationTrace.h"
#include "NavigationScenarioRuntime.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace trace = elite::tools::navigation_runtime;

namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kVelocityVectorMetersPerMps = 3.5f;

#ifndef ELITE_NAV_VIEWER_REVISION
#define ELITE_NAV_VIEWER_REVISION "unknown"
#endif
constexpr const char* kViewerRevision = ELITE_NAV_VIEWER_REVISION;

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
    float alpha = 1.0f;
};

glm::vec3 toVec3(const glm::dvec3& v)
{
    return {
        static_cast<float>(v.x),
        static_cast<float>(v.y),
        static_cast<float>(v.z)
    };
}

struct Camera
{
    glm::vec3 target {150.0f, 20.0f, 0.0f};
    float yaw = -0.70f;
    float pitch = 0.55f;
    float distance = 420.0f;

    glm::vec3 position() const
    {
        const float cp = std::cos(pitch);
        const glm::vec3 direction {
            cp * std::cos(yaw),
            std::sin(pitch),
            cp * std::sin(yaw)
        };
        return target - direction * distance;
    }

    glm::mat4 view() const
    {
        return glm::lookAt(
            position(),
            target,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }
};

enum class UiAction
{
    None,
    Calculate,
    TogglePlay,
    PreviousFrame,
    NextFrame,
    NextReplan,
    Fit
};

enum class SpeedSliderDrag
{
    None,
    Start,
    Finish
};

struct AppState
{
    Camera camera;
    trace::TraceDocument* traceData = nullptr;
    std::string scenarioPath;
    std::string calculationMessage = "ИЗМЕНИТЕ УСЛОВИЯ И НАЖМИТЕ РАССЧИТАТЬ";
    std::vector<std::string> diagnosticLines;

    // Last successful Stage-1 route. Pilot/control-law changes may reuse it,
    // while speed/style changes invalidate it because they change maneuver
    // clearance around static geometry.
    trace::TraceDocument retainedRoute;
    std::vector<std::string> retainedRouteDiagnostics;
    std::string retainedRouteMessage;
    bool hasRetainedRoute = false;

    bool calculationPerformed = false;
    bool calculationSucceeded = false;
    bool executionPerformed = false;
    bool executionSucceeded = false;

    // Calculate has deterministic semantics: it is enabled only while some
    // input invalidates the currently displayed result. Route-affecting dirty
    // inputs require Stage-1+Stage-2; execution-only dirty inputs reuse Stage-1.
    bool routeInputsDirty = true;
    bool executionInputsDirty = true;
    bool calculationInProgress = false;
    std::vector<std::string> shortCalculationLog;

    // Canonical viewer store. UI controls are projections of this state, not
    // independent booleans owned by widgets.
    elite::tools::navigation_runtime::ControlMode controlMode =
        elite::tools::navigation_runtime::ControlMode::Newtonian;
    elite::tools::navigation_runtime::PilotLevel pilot =
        elite::tools::navigation_runtime::PilotLevel::Expert;
    elite::tools::navigation_runtime::FlightStyle flightStyle =
        elite::tools::navigation_runtime::FlightStyle::Standard;
    bool useSuddenObstacle = false;
    double startSpeedMps = 10.0;
    double finishSpeedMps = 10.0;

    bool orbiting = false;
    bool panning = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    bool playing = true;
    std::size_t frameIndex = 0;
    double playbackTime = 0.0;
    double lastRealTime = 0.0;

    bool requestFit = true;
    bool scrubbingFrames = false;
    SpeedSliderDrag speedSliderDrag = SpeedSliderDrag::None;
    UiAction pendingUiAction = UiAction::None;
};

enum class ViewerActionType
{
    SetControlMode,
    SetPilot,
    SetFlightStyle,
    SetStartSpeed,
    SetFinishSpeed,
    ToggleSuddenObstacle,
    QueueUiAction,
    SetPlaying,
    RequestFit,
    RuntimeControlLawObserved
};

struct ViewerAction
{
    ViewerActionType type = ViewerActionType::QueueUiAction;
    elite::tools::navigation_runtime::ControlMode controlMode =
        elite::tools::navigation_runtime::ControlMode::Newtonian;
    elite::tools::navigation_runtime::PilotLevel pilot =
        elite::tools::navigation_runtime::PilotLevel::Expert;
    elite::tools::navigation_runtime::FlightStyle flightStyle =
        elite::tools::navigation_runtime::FlightStyle::Standard;
    UiAction uiAction = UiAction::None;
    bool boolValue = false;
    double speedMps = 10.0;
    std::string runtimeControlLaw;
};

GLuint compileShader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE)
        return shader;

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("shader compile failed: " + log);
}

GLuint makeProgram()
{
    static constexpr const char* VertexShader = R"(
#version 330 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inAlpha;
uniform mat4 uViewProjection;
out vec3 vertexColor;
out float vertexAlpha;
void main()
{
    vertexColor = inColor;
    vertexAlpha = inAlpha;
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
)";

    static constexpr const char* FragmentShader = R"(
#version 330 core
in vec3 vertexColor;
in float vertexAlpha;
out vec4 outColor;
void main()
{
    outColor = vec4(vertexColor, vertexAlpha);
}
)";

    const GLuint vs = compileShader(GL_VERTEX_SHADER, VertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, FragmentShader);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE)
        return program;

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error("shader link failed: " + log);
}

class PrimitiveRenderer
{
public:
    PrimitiveRenderer()
        : program_(makeProgram())
    {
        uniformViewProjection_ =
            glGetUniformLocation(program_, "uViewProjection");

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, position))
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, color))
        );

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            1,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, alpha))
        );

        glBindVertexArray(0);
    }

    ~PrimitiveRenderer()
    {
        if (vbo_ != 0)
            glDeleteBuffers(1, &vbo_);
        if (vao_ != 0)
            glDeleteVertexArrays(1, &vao_);
        if (program_ != 0)
            glDeleteProgram(program_);
    }

    void begin(const glm::mat4& viewProjection)
    {
        glUseProgram(program_);
        glUniformMatrix4fv(
            uniformViewProjection_,
            1,
            GL_FALSE,
            glm::value_ptr(viewProjection)
        );
        glBindVertexArray(vao_);
    }

    void draw(
        GLenum primitive,
        const std::vector<Vertex>& vertices,
        float widthOrSize = 1.0f
    )
    {
        if (vertices.empty())
            return;

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
            vertices.data(),
            GL_STREAM_DRAW
        );

        if (primitive == GL_POINTS)
            glPointSize(widthOrSize);
        else
            glLineWidth(widthOrSize);

        glDrawArrays(
            primitive,
            0,
            static_cast<GLsizei>(vertices.size())
        );
    }

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLint uniformViewProjection_ = -1;
};

void addLine(
    std::vector<Vertex>& out,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& color,
    float alpha = 1.0f
)
{
    out.push_back({a, color, alpha});
    out.push_back({b, color, alpha});
}

std::vector<Vertex> polyline(
    const std::vector<glm::dvec3>& points,
    const glm::vec3& color,
    std::size_t lastInclusive =
        std::numeric_limits<std::size_t>::max()
)
{
    std::vector<Vertex> out;
    if (points.size() < 2)
        return out;

    const std::size_t limit =
        lastInclusive == std::numeric_limits<std::size_t>::max()
            ? points.size()
            : std::min(lastInclusive + 1, points.size());
    for (std::size_t i = 1; i < limit; ++i)
        addLine(out, toVec3(points[i - 1]), toVec3(points[i]), color);

    return out;
}

void appendCross(
    std::vector<Vertex>& out,
    const glm::vec3& center,
    float size,
    const glm::vec3& color
)
{
    addLine(out, center - glm::vec3(size, 0.0f, 0.0f),
                 center + glm::vec3(size, 0.0f, 0.0f), color);
    addLine(out, center - glm::vec3(0.0f, size, 0.0f),
                 center + glm::vec3(0.0f, size, 0.0f), color);
    addLine(out, center - glm::vec3(0.0f, 0.0f, size),
                 center + glm::vec3(0.0f, 0.0f, size), color);
}

void appendReferenceGrid(
    std::vector<Vertex>& out,
    const glm::dvec3& start,
    const glm::dvec3& finish
)
{
    const glm::vec3 a = toVec3(start);
    const glm::vec3 b = toVec3(finish);
    const glm::vec3 center = 0.5f * (a + b);

    const float span =
        std::max(100.0f, glm::length(b - a) * 1.25f);
    const float spacing = 25.0f;
    const int halfLines =
        std::clamp(
            static_cast<int>(std::ceil(span / spacing * 0.5f)),
            4,
            24
        );
    const float halfSize =
        static_cast<float>(halfLines) * spacing;

    const glm::vec3 minor(0.12f, 0.14f, 0.17f);
    const glm::vec3 axisX(0.24f, 0.16f, 0.16f);
    const glm::vec3 axisZ(0.16f, 0.20f, 0.26f);

    for (int i = -halfLines; i <= halfLines; ++i)
    {
        const float d = static_cast<float>(i) * spacing;
        const glm::vec3 xColor = i == 0 ? axisZ : minor;
        const glm::vec3 zColor = i == 0 ? axisX : minor;

        addLine(
            out,
            {center.x - halfSize, 0.0f, center.z + d},
            {center.x + halfSize, 0.0f, center.z + d},
            xColor
        );
        addLine(
            out,
            {center.x + d, 0.0f, center.z - halfSize},
            {center.x + d, 0.0f, center.z + halfSize},
            zColor
        );
    }
}

void appendCircle(
    std::vector<Vertex>& out,
    const glm::vec3& center,
    float radius,
    const glm::vec3& axisA,
    const glm::vec3& axisB,
    const glm::vec3& color,
    int segments = 64
)
{
    if (radius <= 0.0f)
        return;

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            2.0f * kPi * static_cast<float>(i) /
            static_cast<float>(segments);
        const float a1 =
            2.0f * kPi * static_cast<float>(i + 1) /
            static_cast<float>(segments);

        const glm::vec3 p0 =
            center +
            radius * (std::cos(a0) * axisA + std::sin(a0) * axisB);
        const glm::vec3 p1 =
            center +
            radius * (std::cos(a1) * axisA + std::sin(a1) * axisB);

        addLine(out, p0, p1, color);
    }
}

void appendWireSphere(
    std::vector<Vertex>& out,
    const glm::vec3& center,
    float radius,
    const glm::vec3& color
);

void appendWireBox(
    std::vector<Vertex>& out,
    const glm::vec3& center,
    const glm::vec3& half,
    const glm::vec3& color
)
{
    glm::vec3 corners[8];
    int index = 0;
    for (int sx : {-1, 1})
    {
        for (int sy : {-1, 1})
        {
            for (int sz : {-1, 1})
            {
                corners[index++] =
                    center +
                    glm::vec3(
                        static_cast<float>(sx) * half.x,
                        static_cast<float>(sy) * half.y,
                        static_cast<float>(sz) * half.z
                    );
            }
        }
    }

    static constexpr int edges[][2] = {
        {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
        {2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
    };

    for (const auto& edge : edges)
        addLine(out, corners[edge[0]], corners[edge[1]], color);
}

void appendWireCapsuleZ(
    std::vector<Vertex>& out,
    const glm::vec3& center,
    float radius,
    float halfLength,
    const glm::vec3& color
)
{
    const glm::vec3 a =
        center + glm::vec3(0.0f, 0.0f, halfLength);
    const glm::vec3 b =
        center - glm::vec3(0.0f, 0.0f, halfLength);

    appendWireSphere(out, a, radius, color);
    appendWireSphere(out, b, radius, color);

    addLine(
        out,
        a + glm::vec3(radius, 0.0f, 0.0f),
        b + glm::vec3(radius, 0.0f, 0.0f),
        color
    );
    addLine(
        out,
        a - glm::vec3(radius, 0.0f, 0.0f),
        b - glm::vec3(radius, 0.0f, 0.0f),
        color
    );
    addLine(
        out,
        a + glm::vec3(0.0f, radius, 0.0f),
        b + glm::vec3(0.0f, radius, 0.0f),
        color
    );
    addLine(
        out,
        a - glm::vec3(0.0f, radius, 0.0f),
        b - glm::vec3(0.0f, radius, 0.0f),
        color
    );
}

void appendWireSphere(
    std::vector<Vertex>& out,
    const glm::vec3& center,
    float radius,
    const glm::vec3& color
)
{
    appendCircle(
        out, center, radius,
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        color
    );
    appendCircle(
        out, center, radius,
        {1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        color
    );
    appendCircle(
        out, center, radius,
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        color
    );
}

glm::vec3 normalizedOr(
    const glm::dvec3& value,
    const glm::vec3& fallback
)
{
    glm::vec3 v = toVec3(value);
    if (glm::length(v) <= 1.0e-6f)
        return fallback;
    return glm::normalize(v);
}

void appendShipBoxAndArrow(
    std::vector<Vertex>& out,
    const trace::TraceFrame& frame,
    const glm::dvec3& halfExtents
)
{
    const glm::vec3 center = toVec3(frame.shipPosition);
    const glm::vec3 forward =
        normalizedOr(frame.shipForward, {1.0f, 0.0f, 0.0f});
    const glm::vec3 right =
        normalizedOr(frame.shipRight, {0.0f, 0.0f, 1.0f});
    const glm::vec3 up =
        normalizedOr(frame.shipUp, {0.0f, 1.0f, 0.0f});

    const float hx = static_cast<float>(halfExtents.x);
    const float hy = static_cast<float>(halfExtents.y);
    const float hz = static_cast<float>(halfExtents.z);

    // Simple Cobra-like diagnostic silhouette: a triangular planform prism.
    // It makes hull attitude readable without the large nose arrow obscuring
    // the actual velocity vector.
    glm::vec3 prism[6];
    int index = 0;
    for (int sy : {-1, 1})
    {
        const glm::vec3 layer = center + up * (static_cast<float>(sy) * hy);
        prism[index++] = layer + forward * hz;
        prism[index++] = layer - forward * hz - right * hx;
        prism[index++] = layer - forward * hz + right * hx;
    }

    const glm::vec3 hullColor(0.70f, 0.88f, 0.72f);
    static constexpr int prismEdges[][2] = {
        {0,1},{1,2},{2,0},
        {3,4},{4,5},{5,3},
        {0,3},{1,4},{2,5}
    };
    for (const auto& edge : prismEdges)
        addLine(out, prism[edge[0]], prism[edge[1]], hullColor);

    // Nose direction remains visible, but deliberately small and translucent.
    const glm::vec3 arrowColor(0.25f, 0.85f, 1.0f);
    const glm::vec3 nose = center + forward * hz;
    const glm::vec3 tip = nose + forward * 4.5f;
    addLine(out, nose, tip, arrowColor, 0.38f);

    const float wing = 1.5f;
    const glm::vec3 wingBase = tip - forward * 2.0f;
    addLine(out, tip, wingBase + right * wing, arrowColor, 0.38f);
    addLine(out, tip, wingBase - right * wing, arrowColor, 0.38f);
}

void appendMainEngineThrustFace(
    std::vector<Vertex>& out,
    const trace::TraceFrame& frame,
    const glm::dvec3& halfExtents
)
{
    const float throttle =
        static_cast<float>(
            std::clamp(frame.mainEngineThrottle01, 0.0, 1.0)
        );
    if (throttle <= 0.01f)
        return;

    const glm::vec3 center = toVec3(frame.shipPosition);
    const glm::vec3 forward =
        normalizedOr(frame.shipForward, {1.0f, 0.0f, 0.0f});
    const glm::vec3 right =
        normalizedOr(frame.shipRight, {0.0f, 0.0f, 1.0f});
    const glm::vec3 up =
        normalizedOr(frame.shipUp, {0.0f, 1.0f, 0.0f});

    const float hx = static_cast<float>(halfExtents.x);
    const float hy = static_cast<float>(halfExtents.y);
    const float hz = static_cast<float>(halfExtents.z);

    // Rear rectangular face of the diagnostic triangular prism. Slightly
    // offset aft to avoid depth fighting with the hull wireframe.
    const glm::vec3 rearCenter =
        center - forward * (hz + 0.02f);
    const glm::vec3 a = rearCenter - right * hx - up * hy;
    const glm::vec3 b = rearCenter + right * hx - up * hy;
    const glm::vec3 c0 = rearCenter + right * hx + up * hy;
    const glm::vec3 d = rearCenter - right * hx + up * hy;

    const glm::vec3 color(
        1.0f,
        0.36f + 0.34f * throttle,
        0.06f
    );
    const float alpha =
        0.22f + 0.68f * throttle;

    out.push_back({a, color, alpha});
    out.push_back({b, color, alpha});
    out.push_back({c0, color, alpha});

    out.push_back({a, color, alpha});
    out.push_back({c0, color, alpha});
    out.push_back({d, color, alpha});
}

void appendVelocityVector(
    std::vector<Vertex>& out,
    const trace::TraceFrame& frame
)
{
    const double speed = glm::length(frame.shipVelocity);
    if (speed <= 0.05)
        return;

    const glm::vec3 start = toVec3(frame.shipPosition);
    const glm::vec3 direction =
        normalizedOr(frame.shipVelocity, {1.0f, 0.0f, 0.0f});
    const float length =
        static_cast<float>(speed) * kVelocityVectorMetersPerMps;
    const glm::vec3 tip = start + direction * length;

    glm::vec3 seed(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(seed, direction)) > 0.90f)
        seed = {1.0f, 0.0f, 0.0f};
    const glm::vec3 side =
        glm::normalize(glm::cross(direction, seed));

    const glm::vec3 velocityColor(1.0f, 0.78f, 0.10f);
    addLine(out, start, tip, velocityColor);

    const float headBack = std::clamp(length * 0.22f, 2.5f, 6.0f);
    const float headWing = std::clamp(length * 0.10f, 1.5f, 3.5f);
    const glm::vec3 base = tip - direction * headBack;
    addLine(out, tip, base + side * headWing, velocityColor);
    addLine(out, tip, base - side * headWing, velocityColor);
}


void appendReferenceOrientationArrow(
    std::vector<Vertex>& out,
    const trace::TraceFrame& frame,
    const glm::dvec3& halfExtents
)
{
    if (!frame.hasProgramReference)
        return;

    const glm::vec3 center = toVec3(frame.shipPosition);
    const glm::vec3 forward =
        normalizedOr(
            frame.programReferenceForward,
            {1.0f, 0.0f, 0.0f}
        );

    const glm::vec3 right =
        normalizedOr(
            frame.programReferenceRight,
            {0.0f, 0.0f, 1.0f}
        );
    const float hz = static_cast<float>(halfExtents.z);
    const glm::vec3 tip =
        center + forward * (hz + 7.0f);
    const glm::vec3 color(1.0f, 0.25f, 0.18f);
    addLine(out, center, tip, color);

    const glm::vec3 headBase = tip - forward * 4.0f;
    addLine(out, tip, headBase + right * 2.5f, color);
    addLine(out, tip, headBase - right * 2.5f, color);
}

void appendTrackingTube(
    std::vector<Vertex>& out,
    const std::vector<glm::dvec3>& path,
    float radius,
    const glm::vec3& color,
    float alpha
)
{
    constexpr int Sides = 12;
    if (path.size() < 2 || radius <= 0.0f)
        return;

    std::vector<std::array<glm::vec3, Sides>> rings;
    rings.resize(path.size());

    for (std::size_t i = 0; i < path.size(); ++i)
    {
        glm::vec3 tangent;
        if (i == 0)
            tangent = toVec3(path[1] - path[0]);
        else if (i + 1 == path.size())
            tangent = toVec3(path[i] - path[i - 1]);
        else
            tangent = toVec3(path[i + 1] - path[i - 1]);

        if (glm::length(tangent) <= 1.0e-6f)
            tangent = {1.0f, 0.0f, 0.0f};
        tangent = glm::normalize(tangent);

        glm::vec3 seed(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(seed, tangent)) > 0.92f)
            seed = {1.0f, 0.0f, 0.0f};

        const glm::vec3 right =
            glm::normalize(glm::cross(tangent, seed));
        const glm::vec3 up =
            glm::normalize(glm::cross(right, tangent));
        const glm::vec3 center = toVec3(path[i]);

        for (int side = 0; side < Sides; ++side)
        {
            const float a =
                2.0f * kPi *
                static_cast<float>(side) /
                static_cast<float>(Sides);
            rings[i][static_cast<std::size_t>(side)] =
                center +
                radius *
                    (std::cos(a) * right + std::sin(a) * up);
        }
    }

    for (std::size_t i = 1; i < rings.size(); ++i)
    {
        for (int side = 0; side < Sides; ++side)
        {
            const int next = (side + 1) % Sides;
            const glm::vec3& a =
                rings[i - 1][static_cast<std::size_t>(side)];
            const glm::vec3& b =
                rings[i - 1][static_cast<std::size_t>(next)];
            const glm::vec3& c =
                rings[i][static_cast<std::size_t>(next)];
            const glm::vec3& d =
                rings[i][static_cast<std::size_t>(side)];

            out.push_back({a, color, alpha});
            out.push_back({b, color, alpha});
            out.push_back({c, color, alpha});

            out.push_back({a, color, alpha});
            out.push_back({c, color, alpha});
            out.push_back({d, color, alpha});
        }
    }
}

trace::TraceFrame interpolatedDisplayFrame(
    const trace::TraceDocument& data,
    const AppState& state
)
{
    if (data.frames.empty())
        return {};

    const std::size_t i =
        std::min(state.frameIndex, data.frames.size() - 1);
    trace::TraceFrame out = data.frames[i];

    if (!state.playing || i + 1 >= data.frames.size())
        return out;

    const double desiredTime =
        data.frames.front().timeSeconds +
        state.playbackTime;
    const auto& a = data.frames[i];
    const auto& b = data.frames[i + 1];
    const double dt = b.timeSeconds - a.timeSeconds;
    if (dt <= 1.0e-9)
        return out;

    const double t =
        std::clamp(
            (desiredTime - a.timeSeconds) / dt,
            0.0,
            1.0
        );

    auto lerp3 = [t](const glm::dvec3& x, const glm::dvec3& y)
    {
        return x + (y - x) * t;
    };

    auto nlerp3 = [&](const glm::dvec3& x, const glm::dvec3& y)
    {
        glm::dvec3 v = lerp3(x, y);
        const double len = glm::length(v);
        if (len <= 1.0e-12)
            return x;
        return v / len;
    };

    out.timeSeconds = desiredTime;
    out.shipPosition = lerp3(a.shipPosition, b.shipPosition);
    out.shipVelocity = lerp3(a.shipVelocity, b.shipVelocity);
    out.shipForward = nlerp3(a.shipForward, b.shipForward);
    out.shipRight = nlerp3(a.shipRight, b.shipRight);
    out.shipUp = nlerp3(a.shipUp, b.shipUp);
    out.mainEngineThrottle01 =
        std::clamp(
            a.mainEngineThrottle01 +
                (b.mainEngineThrottle01 -
                 a.mainEngineThrottle01) * t,
            0.0,
            1.0
        );

    if (a.hasRuntimeControlLaw || b.hasRuntimeControlLaw)
    {
        const trace::TraceFrame& lawFrame =
            t < 0.5 ? a : b;
        const trace::TraceFrame& fallbackLawFrame =
            t < 0.5 ? b : a;

        if (lawFrame.hasRuntimeControlLaw)
        {
            out.hasRuntimeControlLaw = true;
            out.runtimeControlLaw =
                lawFrame.runtimeControlLaw;
        }
        else if (fallbackLawFrame.hasRuntimeControlLaw)
        {
            out.hasRuntimeControlLaw = true;
            out.runtimeControlLaw =
                fallbackLawFrame.runtimeControlLaw;
        }
    }

    if (a.hazardActive && b.hazardActive)
    {
        out.hazardPosition =
            lerp3(a.hazardPosition, b.hazardPosition);
        out.hazardVelocity =
            lerp3(a.hazardVelocity, b.hazardVelocity);
        out.plannerLookAheadSeconds =
            a.plannerLookAheadSeconds +
            (b.plannerLookAheadSeconds - a.plannerLookAheadSeconds) * t;
        out.dynamicClearanceMeters =
            a.dynamicClearanceMeters +
            (b.dynamicClearanceMeters - a.dynamicClearanceMeters) * t;
    }

    if (a.hasProgramReference && b.hasProgramReference)
    {
        out.hasProgramReference = true;
        out.programReferencePosition =
            lerp3(a.programReferencePosition, b.programReferencePosition);
        out.programReferenceVelocity =
            lerp3(a.programReferenceVelocity, b.programReferenceVelocity);
        out.programSpeedCorridorHalfWidthMps =
            a.programSpeedCorridorHalfWidthMps +
            (b.programSpeedCorridorHalfWidthMps -
             a.programSpeedCorridorHalfWidthMps) * t;
        out.programProgressCorridorHalfWidthMeters =
            a.programProgressCorridorHalfWidthMeters +
            (b.programProgressCorridorHalfWidthMeters -
             a.programProgressCorridorHalfWidthMeters) * t;
        out.programReferenceForward =
            nlerp3(a.programReferenceForward, b.programReferenceForward);
        out.programReferenceRight =
            nlerp3(a.programReferenceRight, b.programReferenceRight);
        out.programReferenceUp =
            nlerp3(a.programReferenceUp, b.programReferenceUp);
        out.programTrackingCorridorRadiusMeters =
            a.programTrackingCorridorRadiusMeters +
            (b.programTrackingCorridorRadiusMeters -
             a.programTrackingCorridorRadiusMeters) * t;
    }

    return out;
}


struct UiRect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(double px, double py) const
    {
        return
            px >= x && px <= x + width &&
            py >= y && py <= y + height;
    }
};

UiRect assistedRect() { return {16.0f, 34.0f, 108.0f, 28.0f}; }
UiRect newtonianRect() { return {128.0f, 34.0f, 132.0f, 28.0f}; }

UiRect expertRect() { return {280.0f, 34.0f, 92.0f, 28.0f}; }
UiRect averageRect() { return {376.0f, 34.0f, 96.0f, 28.0f}; }
UiRect loserRect() { return {476.0f, 34.0f, 92.0f, 28.0f}; }

UiRect standardRect() { return {588.0f, 34.0f, 110.0f, 28.0f}; }
UiRect extremeRect() { return {702.0f, 34.0f, 108.0f, 28.0f}; }

UiRect suddenObstacleRect() { return {830.0f, 34.0f, 210.0f, 28.0f}; }
UiRect calculateButtonRect() { return {1052.0f, 30.0f, 160.0f, 34.0f}; }

UiRect playButtonRect() { return {16.0f, 78.0f, 184.0f, 32.0f}; }
UiRect prevButtonRect() { return {208.0f, 78.0f, 86.0f, 32.0f}; }
UiRect nextButtonRect() { return {302.0f, 78.0f, 94.0f, 32.0f}; }
UiRect replanButtonRect() { return {404.0f, 78.0f, 194.0f, 32.0f}; }
UiRect fitButtonRect() { return {606.0f, 78.0f, 94.0f, 32.0f}; }

UiRect startSpeedSliderRect()
{
    return {16.0f, 138.0f, 300.0f, 20.0f};
}

UiRect finishSpeedSliderRect()
{
    return {370.0f, 138.0f, 300.0f, 20.0f};
}

double speedFromSliderX(
    const UiRect& rect,
    double mouseX
)
{
    constexpr double MinimumSpeedMps = 5.0;
    constexpr double MaximumSpeedMps = 50.0;
    const double u = std::clamp(
        (mouseX - static_cast<double>(rect.x)) /
            static_cast<double>(rect.width),
        0.0,
        1.0
    );
    return
        MinimumSpeedMps +
        (MaximumSpeedMps - MinimumSpeedMps) * u;
}

UiRect frameSliderRect(int windowWidth, int windowHeight)
{
    const float sidePanel = 470.0f;
    const float width =
        std::max(
            180.0f,
            static_cast<float>(windowWidth) - sidePanel - 48.0f
        );
    return {
        16.0f,
        static_cast<float>(windowHeight) - 38.0f,
        width,
        16.0f
    };
}

std::array<std::uint8_t, 7> glyphRows(std::uint32_t c)
{
    if (c >= 'a' && c <= 'z')
        c = c - 'a' + 'A';
    if (c >= 0x0430 && c <= 0x044F)
        c -= 0x20;
    if (c == 0x0451)
        c = 0x0401;

    switch (c)
    {
        case 'A': return {14,17,17,31,17,17,17};
        case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14};
        case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31};
        case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,15};
        case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {31,4,4,4,4,4,31};
        case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17};
        case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17};
        case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14};
        case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13};
        case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};
        case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14};
        case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10};
        case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14};
        case ':': return {0,4,4,0,4,4,0};
        case '.': return {0,0,0,0,0,6,6};
        case ',': return {0,0,0,0,6,6,4};
        case '-': return {0,0,0,31,0,0,0};
        case '+': return {0,4,4,31,4,4,0};
        case '/': return {1,2,2,4,8,8,16};
        case '[': return {14,8,8,8,8,8,14};
        case ']': return {14,2,2,2,2,2,14};
        case '(': return {2,4,8,8,8,4,2};
        case ')': return {8,4,2,2,2,4,8};
        case '_': return {0,0,0,0,0,0,31};
        case '=': return {0,31,0,31,0,0,0};
        case '>': return {16,8,4,2,4,8,16};
        case '<': return {1,2,4,8,4,2,1};
        case '!': return {4,4,4,4,4,0,4};
        case '?': return {14,17,1,2,4,0,4};

        // Cyrillic uppercase 5x7 glyphs used by the Russian diagnostic UI.
        case 0x0410: return {14,17,17,31,17,17,17}; // А
        case 0x0411: return {31,16,16,30,17,17,30}; // Б
        case 0x0412: return {30,17,17,30,17,17,30}; // В
        case 0x0413: return {31,16,16,16,16,16,16}; // Г
        case 0x0414: return {14,10,10,10,10,31,17}; // Д
        case 0x0415: return {31,16,16,30,16,16,31}; // Е
        case 0x0401: return {10,0,31,16,30,16,31};  // Ё
        case 0x0416: return {21,21,14,31,14,21,21}; // Ж
        case 0x0417: return {30,1,1,14,1,1,30};     // З
        case 0x0418: return {17,19,21,25,17,17,17}; // И
        case 0x0419: return {10,4,17,19,21,25,17};  // Й
        case 0x041A: return {17,18,20,24,20,18,17}; // К
        case 0x041B: return {3,5,9,17,17,17,17};    // Л
        case 0x041C: return {17,27,21,21,17,17,17}; // М
        case 0x041D: return {17,17,17,31,17,17,17}; // Н
        case 0x041E: return {14,17,17,17,17,17,14}; // О
        case 0x041F: return {31,17,17,17,17,17,17}; // П
        case 0x0420: return {30,17,17,30,16,16,16}; // Р
        case 0x0421: return {14,17,16,16,16,17,14}; // С
        case 0x0422: return {31,4,4,4,4,4,4};        // Т
        case 0x0423: return {17,17,17,15,1,17,14};  // У
        case 0x0424: return {4,14,21,21,14,4,4};    // Ф
        case 0x0425: return {17,17,10,4,10,17,17};  // Х
        case 0x0426: return {17,17,17,17,17,31,1};  // Ц
        case 0x0427: return {17,17,17,15,1,1,1};    // Ч
        case 0x0428: return {21,21,21,21,21,21,31}; // Ш
        case 0x0429: return {21,21,21,21,21,31,1};  // Щ
        case 0x042A: return {24,8,8,14,9,9,14};     // Ъ
        case 0x042B: return {17,17,17,29,21,21,29}; // Ы
        case 0x042C: return {16,16,16,30,17,17,30}; // Ь
        case 0x042D: return {14,17,1,7,1,17,14};    // Э
        case 0x042E: return {18,21,21,29,21,21,18}; // Ю
        case 0x042F: return {15,17,17,15,5,9,17};   // Я

        default: return {0,0,0,0,0,0,0};
    }
}

void appendFilledRect(
    std::vector<Vertex>& out,
    const UiRect& rect,
    const glm::vec3& color
)
{
    const glm::vec3 a(rect.x, rect.y, 0.0f);
    const glm::vec3 b(rect.x + rect.width, rect.y, 0.0f);
    const glm::vec3 c(rect.x + rect.width, rect.y + rect.height, 0.0f);
    const glm::vec3 d(rect.x, rect.y + rect.height, 0.0f);

    out.push_back({a, color}); out.push_back({b, color}); out.push_back({c, color});
    out.push_back({a, color}); out.push_back({c, color}); out.push_back({d, color});
}

void appendUiText(
    std::vector<Vertex>& out,
    float x,
    float y,
    const std::string& text,
    float scale,
    const glm::vec3& color
)
{
    float cursorX = x;
    float cursorY = y;
    const float pixel = scale;
    const float advance = 6.0f * scale;
    const float lineAdvance = 9.0f * scale;

    for (std::size_t offset = 0; offset < text.size();)
    {
        const unsigned char lead =
            static_cast<unsigned char>(text[offset]);
        std::uint32_t ch = 0;
        std::size_t consumed = 1;

        if (lead < 0x80)
        {
            ch = lead;
        }
        else if ((lead & 0xE0) == 0xC0 &&
                 offset + 1 < text.size())
        {
            ch =
                ((lead & 0x1F) << 6) |
                (static_cast<unsigned char>(text[offset + 1]) & 0x3F);
            consumed = 2;
        }
        else if ((lead & 0xF0) == 0xE0 &&
                 offset + 2 < text.size())
        {
            ch =
                ((lead & 0x0F) << 12) |
                ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 6) |
                (static_cast<unsigned char>(text[offset + 2]) & 0x3F);
            consumed = 3;
        }
        else
        {
            ch = '?';
        }

        offset += consumed;

        if (ch == '\n')
        {
            cursorX = x;
            cursorY += lineAdvance;
            continue;
        }

        if (ch == ' ')
        {
            cursorX += advance;
            continue;
        }

        const auto rows = glyphRows(ch);
        for (int row = 0; row < 7; ++row)
        {
            for (int col = 0; col < 5; ++col)
            {
                const std::uint8_t bit =
                    static_cast<std::uint8_t>(1u << (4 - col));
                if ((rows[static_cast<std::size_t>(row)] & bit) == 0)
                    continue;

                appendFilledRect(
                    out,
                    {
                        cursorX + static_cast<float>(col) * pixel,
                        cursorY + static_cast<float>(row) * pixel,
                        pixel,
                        pixel
                    },
                    color
                );
            }
        }

        cursorX += advance;
    }
}

void appendUiCircle(
    std::vector<Vertex>& out,
    const glm::vec2& center,
    float radius,
    const glm::vec3& color,
    float alpha,
    int segments = 28
)
{
    if (radius <= 0.0f)
        return;

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            2.0f * kPi * static_cast<float>(i) /
            static_cast<float>(segments);
        const float a1 =
            2.0f * kPi * static_cast<float>(i + 1) /
            static_cast<float>(segments);

        const glm::vec3 c(center.x, center.y, 0.0f);
        const glm::vec3 p0(
            center.x + radius * std::cos(a0),
            center.y + radius * std::sin(a0),
            0.0f
        );
        const glm::vec3 p1(
            center.x + radius * std::cos(a1),
            center.y + radius * std::sin(a1),
            0.0f
        );

        out.push_back({c, color, alpha});
        out.push_back({p0, color, alpha});
        out.push_back({p1, color, alpha});
    }
}

void appendUiButton(
    std::vector<Vertex>& triangles,
    const UiRect& rect,
    const std::string& label,
    bool active = false,
    bool enabled = true
)
{
    const glm::vec3 background =
        !enabled
            ? glm::vec3(0.065f, 0.070f, 0.080f)
            : active
                ? glm::vec3(0.18f, 0.34f, 0.22f)
                : glm::vec3(0.12f, 0.14f, 0.18f);

    appendFilledRect(
        triangles,
        rect,
        background
    );

    appendUiText(
        triangles,
        rect.x + 9.0f,
        rect.y + 9.0f,
        label,
        1.6f,
        enabled
            ? glm::vec3(0.92f, 0.94f, 0.98f)
            : glm::vec3(0.38f, 0.40f, 0.44f)
    );
}

std::string localizedLaw(const std::string& law)
{
    if (law == "newtonian" || law == "NEWTONIAN")
        return "НЬЮТОНОВСКИЙ";
    if (law == "assisted" || law == "ASSISTED")
        return "АССИСТИРОВАННЫЙ";
    return law;
}

std::string localizedPhase(const std::string& phase)
{
    if (phase == "initial")
        return "СТАРТ";
    if (phase == "route_ready")
        return "ЭТАП 1 — МАРШРУТ";
    if (phase == "route_failed")
        return "ЭТАП 1 — ОШИБКА МАРШРУТА";
    if (phase == "scene_preview")
        return "СЦЕНА — ДО РАСЧЁТА";
    if (phase == "route_execution")
        return "ЭТАП 2 — FOLLOWER";
    if (phase == "phase_handoff")
        return "ЭТАП 2 — ПЕРЕХОД ФАЗЫ";
    if (phase == "execution_complete")
        return "ЭТАП 2 — ЗАВЕРШЕНО";
    if (phase == "execution_failed")
        return "ЭТАП 2 — ОШИБКА";
    if (phase == "portal_101")
        return "ПОРТАЛ 101";
    if (phase == "doctrine_prefix")
        return "ПОЛЁТ К ПОРТАЛУ 102";
    if (phase == "dynamic_replan")
        return "ПЕРЕПЛАНИРОВАНИЕ";
    if (phase.rfind("dynamic_bypass_", 0) == 0)
        return "ОБХОД ПРЕПЯТСТВИЯ";
    if (phase.rfind("dynamic_brake_", 0) == 0)
        return "ТОРМОЖЕНИЕ";
    if (phase.rfind("replan_", 0) == 0)
        return "ПЕРЕПЛАНИРОВАНИЕ";
    if (phase == "portal_102")
        return "ПОРТАЛ 102";
    if (phase == "final_capture")
        return "ФИНАЛЬНОЕ ПОЗИЦИОНИРОВАНИЕ";
    if (phase == "nominal_segment")
        return "НОМИНАЛЬНЫЙ СЕГМЕНТ";
    if (phase == "complete")
        return "РАСЧЁТ ЗАВЕРШЁН";
    if (phase == "failed")
        return "РАСЧЁТ ОСТАНОВЛЕН";
    return phase;
}

std::string localizedStatus(const std::string& status)
{
    if (status.empty())
        return "НЕТ";
    if (status == "nominal_clear")
        return "НОМИНАЛЬНЫЙ ПУТЬ СВОБОДЕН";
    if (status == "adjusted_clear")
        return "ВЫБРАН БЕЗОПАСНЫЙ ОБХОД";
    if (status == "conflict_hold")
        return "КОНФЛИКТ - ТОРМОЖЕНИЕ";
    if (status == "stale_hold")
        return "ДАННЫЕ УСТАРЕЛИ";
    if (status == "static_hold")
        return "СТАТИЧЕСКАЯ ПОМЕХА";
    if (status == "invalid_input")
        return "ОШИБКА ВХОДНЫХ ДАННЫХ";
    if (status == "static_route_ready")
        return "СТАТИЧЕСКИЙ МАРШРУТ ГОТОВ";
    if (status == "static_route_failed")
        return "СТАТИЧЕСКИЙ МАРШРУТ НЕ ПОСТРОЕН";
    if (status == "scene_loaded")
        return "СЦЕНА ЗАГРУЖЕНА";
    if (status == "follower_running")
        return "FOLLOWER ВЫПОЛНЯЕТ МАРШРУТ";
    if (status == "trajectory_failed")
        return "RUCKIG НЕ ПОСТРОИЛ ТРАЕКТОРИЮ";
    if (status == "follower_invalid")
        return "FOLLOWER: INVALID INPUT";
    if (status == "pilot_bridge_invalid")
        return "PILOT BRIDGE: ОШИБКА";
    if (status == "static_contact")
        return "КОНТАКТ СО СТАТИЧЕСКИМ ПРЕПЯТСТВИЕМ";
    if (status == "terminal_miss")
        return "ФИНИШНОЕ СОСТОЯНИЕ НЕ ДОСТИГНУТО";
    if (status == "capture_timeout")
        return "ФИНИШНЫЙ ЗАХВАТ: TIMEOUT";
    if (status == "follower_complete")
        return "FOLLOWER: МАРШРУТ ВЫПОЛНЕН";
    return status;
}

double orientationErrorDegrees(const trace::TraceFrame& frame)
{
    if (!frame.hasProgramReference)
        return 0.0;

    const glm::dvec3 a =
        glm::normalize(frame.shipForward);
    const glm::dvec3 b =
        glm::normalize(frame.programReferenceForward);
    const double dot =
        std::clamp(glm::dot(a, b), -1.0, 1.0);
    return std::acos(dot) * 180.0 / 3.14159265358979323846;
}

std::string currentExplanation(const trace::TraceFrame& frame)
{
    if (frame.phase == "scene_preview")
        return "СЦЕНА ЗАГРУЖЕНА. PLANNER ЕЩЁ НЕ ЗАПУСКАЛСЯ.\nFOLLOWER ЕЩЁ НЕ ЗАПУСКАЛСЯ.";

    if (frame.phase == "route_execution")
        return "ЭТАП 2: FOLLOWER ИСПОЛНЯЕТ СОХРАНЁННЫЙ МАРШРУТ.\nГЛОБАЛЬНЫЙ PLANNER НЕ ПЕРЕСЧИТЫВАЕТСЯ.";

    if (frame.phase == "execution_complete")
        return "ЭТАП 2: КОБРА ПРОШЛА СОХРАНЁННЫЙ МАРШРУТ.\nСМОТРИТЕ ФАКТИЧЕСКУЮ ТРАЕКТОРИЮ И ОШИБКИ.";

    if (frame.phase == "execution_failed")
        return "ЭТАП 2: ИСПОЛНЕНИЕ ЗАВЕРШИЛОСЬ ОШИБКОЙ.\nСМОТРИТЕ ЦЕПОЧКУ И last_execution.log.";

    if (frame.phase == "route_ready")
        return "ЭТАП 1: ПОСТРОЕН ОДИН СТАТИЧЕСКИЙ МАРШРУТ\nСТАРТ -> ФИНИШ. ПОЛЁТ ЕЩЁ НЕ РАССЧИТЫВАЕТСЯ.";

    if (frame.phase == "route_failed")
        return "ЭТАП 1: СТАТИЧЕСКИЙ МАРШРУТ НЕ НАЙДЕН.\nДИНАМИЧЕСКИЕ ОБЪЕКТЫ ЗДЕСЬ НЕ УЧАСТВУЮТ.";

    if (frame.phase == "dynamic_replan")
        return "ПОМЕХА ПЕРЕСЕКЛА ПРИНЯТЫЙ ПУТЬ -\nЛОКАЛЬНОЕ ПЕРЕПЛАНИРОВАНИЕ";

    if (frame.phase.rfind("dynamic_bypass_", 0) == 0)
        return "КОБРА ВЫПОЛНЯЕТ ФИЗИЧЕСКИ ДОПУСТИМЫЙ\nЛОКАЛЬНЫЙ ОБХОД";

    if (frame.phase.rfind("dynamic_brake_", 0) == 0)
        return "БЕЗОПАСНЫЙ ОБХОД НЕДОСТУПЕН -\nАКТИВНОЕ ТОРМОЖЕНИЕ";

    if (frame.phase.rfind("replan_", 0) == 0)
    {
        if (frame.plannerStatus == "adjusted_clear")
            return "ПОМЕХА ЕЩЁ ПЕРЕКРЫВАЕТ НОМИНАЛЬНЫЙ СЕГМЕНТ -\nСТРОИТСЯ ОБХОД";
        if (frame.plannerStatus == "nominal_clear")
            return "СЛЕДУЮЩИЙ КОРОТКИЙ СЕГМЕНТ СВОБОДЕН -\nВОЗВРАТ К МАРШРУТУ";
        return "РЕЗУЛЬТАТ ЛОКАЛЬНОГО ПЕРЕПЛАНИРОВАНИЯ";
    }

    if (frame.phase == "portal_101")
        return "ПОЛЁТ ПО СТАТИЧЕСКОМУ МАРШРУТУ\nК ПОРТАЛУ 101";

    if (frame.phase == "doctrine_prefix")
        return "ВЫПОЛНЯЕТСЯ ПРИНЯТАЯ ПРОГРАММА ПОЛЁТА\nК ПОРТАЛУ 102";

    if (frame.phase == "portal_102")
        return "ДЛИННЫЙ ПЕРЕХОД К ПОРТАЛУ 102 -\nЗДЕСЬ СЕЙЧАС ТЕРЯЕТСЯ ЗАЗОР";

    if (frame.phase == "final_capture")
        return "ФИНАЛЬНОЕ ТОЧНОЕ ПОЗИЦИОНИРОВАНИЕ";

    if (frame.phase == "nominal_segment")
        return "ПЛАНЕР ВЕДЁТ КОБРУ ПО ТЕКУЩЕМУ\nБЕЗОПАСНОМУ СЕГМЕНТУ";

    if (frame.phase == "complete")
        return "МАРШРУТ РАССЧИТАН И ВЫПОЛНЕН\nДО ФИНАЛЬНОГО СОСТОЯНИЯ";

    if (frame.phase == "failed")
        return "РАСЧЁТ ОСТАНОВЛЕН -\nСМОТРИТЕ ПОСЛЕДНИЙ СТАТУС ПЛАНЕРА";

    return "СТАРТ МАРШРУТА";
}

void appendHorizonInset(
    std::vector<Vertex>& ui,
    const trace::TraceFrame& frame,
    int windowHeight
)
{
    const UiRect panel {
        16.0f,
        static_cast<float>(windowHeight) - 300.0f,
        320.0f,
        220.0f
    };

    appendFilledRect(
        ui,
        panel,
        {0.035f, 0.045f, 0.060f}
    );

    appendUiText(
        ui,
        panel.x + 12.0f,
        panel.y + 10.0f,
        "ГОРИЗОНТ КОБРЫ",
        1.35f,
        {0.90f, 0.94f, 1.0f}
    );

    const glm::vec2 center(
        panel.x + panel.width * 0.5f,
        panel.y + panel.height * 0.56f
    );

    const float pixelsPerMeter = 2.0f;

    // Crosshair: projection plane perpendicular to Cobra longitudinal axis.
    appendFilledRect(
        ui,
        {center.x - 44.0f, center.y - 0.5f, 88.0f, 1.0f},
        {0.30f, 0.34f, 0.42f}
    );
    appendFilledRect(
        ui,
        {center.x - 0.5f, center.y - 44.0f, 1.0f, 88.0f},
        {0.30f, 0.34f, 0.42f}
    );
    appendUiCircle(
        ui,
        center,
        6.0f,
        {0.45f, 0.85f, 1.0f},
        0.85f
    );

    if (!frame.hazardActive)
    {
        appendUiText(
            ui,
            panel.x + 12.0f,
            panel.y + panel.height - 22.0f,
            "ПОМЕХА НЕ АКТИВНА",
            1.20f,
            {0.60f, 0.65f, 0.72f}
        );
        return;
    }

    const glm::dvec3 forward =
        glm::normalize(frame.shipForward);
    const glm::dvec3 right =
        glm::normalize(frame.shipRight);
    const glm::dvec3 up =
        glm::normalize(frame.shipUp);

    const double horizon =
        std::max(0.0, frame.plannerLookAheadSeconds);

    for (int i = 0; i <= 12; ++i)
    {
        const double u = static_cast<double>(i) / 12.0;
        const double t = horizon * u;

        const glm::dvec3 predictedHazard =
            frame.hazardPosition +
            frame.hazardVelocity * t;
        const glm::dvec3 predictedShip =
            frame.shipPosition +
            frame.shipVelocity * t;
        const glm::dvec3 relative =
            predictedHazard - predictedShip;

        const double depth = glm::dot(relative, forward);
        if (depth <= 0.0)
            continue;

        const float px =
            center.x +
            static_cast<float>(glm::dot(relative, right)) *
                pixelsPerMeter;
        const float py =
            center.y -
            static_cast<float>(glm::dot(relative, up)) *
                pixelsPerMeter;

        const float radius =
            static_cast<float>(
                frame.hazardPlannerEnvelopeRadiusMeters
            ) * pixelsPerMeter;

        appendUiCircle(
            ui,
            {px, py},
            radius,
            {1.0f, 0.22f, 0.16f},
            0.055f + 0.12f * static_cast<float>(1.0 - u)
        );
    }

    const glm::dvec3 currentRelative =
        frame.hazardPosition - frame.shipPosition;
    const double currentDepth =
        glm::dot(currentRelative, forward);

    std::ostringstream depthLine;
    depthLine.setf(std::ios::fixed);
    depthLine.precision(1);
    depthLine
        << "ДАЛЬНОСТЬ ВПЕРЁД: "
        << currentDepth
        << " М";
    appendUiText(
        ui,
        panel.x + 12.0f,
        panel.y + panel.height - 22.0f,
        depthLine.str(),
        1.12f,
        currentDepth > 0.0
            ? glm::vec3(0.90f, 0.75f, 0.32f)
            : glm::vec3(0.55f, 0.58f, 0.64f)
    );
}

void drawHud(
    PrimitiveRenderer& renderer,
    const trace::TraceDocument& data,
    const AppState& state,
    const trace::TraceFrame& displayFrame,
    int windowWidth,
    int windowHeight
)
{
    const bool hasScene = !data.frames.empty();
    const bool hasCalculation =
        state.calculationPerformed && hasScene;
    const bool hasExecution =
        data.frames.size() > 1;
    const trace::TraceFrame frame =
        hasScene ? displayFrame : trace::TraceFrame {};

    const glm::mat4 projection =
        glm::ortho(
            0.0f,
            static_cast<float>(windowWidth),
            static_cast<float>(windowHeight),
            0.0f,
            -1.0f,
            1.0f
        );

    glDisable(GL_DEPTH_TEST);
    renderer.begin(projection);

    std::vector<Vertex> ui;

    // Always-visible source stamp. Do not rely only on the native title or the
    // right diagnostics panel: screenshots may crop either of them.
    appendFilledRect(
        ui,
        {
            12.0f,
            92.0f,
            250.0f,
            28.0f
        },
        {0.02f, 0.025f, 0.035f}
    );
    appendUiText(
        ui,
        20.0f,
        99.0f,
        std::string("NAV REV ") + kViewerRevision,
        1.35f,
        {1.0f, 0.82f, 0.28f}
    );

    // Top control bar is independent from the diagnostics panel.
    appendUiText(
        ui, 16.0f, 14.0f,
        "РЕЖИМ УПРАВЛЕНИЯ",
        1.10f,
        {0.72f,0.78f,0.86f}
    );
    appendUiButton(
        ui,
        assistedRect(),
        "АССИСТЕД",
        state.controlMode ==
            elite::tools::navigation_runtime::ControlMode::Assisted
    );
    appendUiButton(
        ui,
        newtonianRect(),
        "НЬЮТОН",
        state.controlMode ==
            elite::tools::navigation_runtime::ControlMode::Newtonian
    );

    appendUiText(
        ui, 280.0f, 14.0f,
        "ПИЛОТ",
        1.10f,
        {0.72f,0.78f,0.86f}
    );
    appendUiButton(
        ui,
        expertRect(),
        "ЭКСПЕРТ",
        state.pilot ==
            elite::tools::navigation_runtime::PilotLevel::Expert
    );
    appendUiButton(
        ui,
        averageRect(),
        "СРЕДНИЙ",
        state.pilot ==
            elite::tools::navigation_runtime::PilotLevel::Average
    );
    appendUiButton(
        ui,
        loserRect(),
        "ЛУЗЕР",
        state.pilot ==
            elite::tools::navigation_runtime::PilotLevel::Loser
    );

    appendUiText(
        ui, 588.0f, 14.0f,
        "РЕЖИМ ПОЛЁТА",
        1.10f,
        {0.72f,0.78f,0.86f}
    );
    appendUiButton(
        ui,
        standardRect(),
        "СТАНДАРТ",
        state.flightStyle ==
            elite::tools::navigation_runtime::FlightStyle::Standard
    );
    appendUiButton(
        ui,
        extremeRect(),
        "ЭКСТРИМ",
        state.flightStyle ==
            elite::tools::navigation_runtime::FlightStyle::Extreme
    );

    appendUiButton(
        ui,
        suddenObstacleRect(),
        state.useSuddenObstacle
            ? "[X] ВНЕЗАПНАЯ ПОМЕХА"
            : "[ ] ВНЕЗАПНАЯ ПОМЕХА",
        state.useSuddenObstacle
    );
    const bool calculateEnabled =
        recalculationRequired(state) &&
        !state.calculationInProgress;
    appendUiButton(
        ui,
        calculateButtonRect(),
        state.calculationInProgress
            ? "РАСЧЕТ..."
            : calculateEnabled
                ? "РАССЧИТАТЬ"
                : "РАСЧЕТ ГОТОВ",
        false,
        calculateEnabled
    );

    if (hasExecution)
    {
        appendUiButton(
            ui,
            playButtonRect(),
            state.playing ? "ПАУЗА" : "ПРОИГРЫВАТЬ",
            state.playing
        );
        appendUiButton(ui, prevButtonRect(), "НАЗАД");
        appendUiButton(ui, nextButtonRect(), "ВПЕРЁД");
        appendUiButton(ui, replanButtonRect(), "СЛЕД. СОБЫТИЕ");
    }
    else if (state.executionPerformed)
    {
        appendUiButton(
            ui,
            playButtonRect(),
            "ПОЛЁТ: ОШИБКА",
            false
        );
        appendUiButton(ui, prevButtonRect(), "НЕТ КАДРОВ");
        appendUiButton(ui, nextButtonRect(), "НЕТ КАДРОВ");
        appendUiButton(ui, replanButtonRect(), "СМОТРИТЕ ЛОГ");
    }
    else
    {
        appendUiButton(
            ui,
            playButtonRect(),
            "СНАЧАЛА РАСЧЁТ",
            false
        );
        appendUiButton(ui, prevButtonRect(), "НЕТ КАДРОВ");
        appendUiButton(ui, nextButtonRect(), "НЕТ КАДРОВ");
        appendUiButton(ui, replanButtonRect(), "FOLLOWER: ОЖИДАЕТ");
    }
    appendUiButton(ui, fitButtonRect(), "ВПИСАТЬ");

    auto appendSpeedSlider = [&](
        const UiRect& rect,
        const char* label,
        double value)
    {
        std::ostringstream text;
        text.setf(std::ios::fixed);
        text.precision(1);
        text << label << ": " << value << " М/С";

        appendUiText(
            ui,
            rect.x,
            rect.y - 17.0f,
            text.str(),
            1.05f,
            {0.78f, 0.84f, 0.92f}
        );

        appendFilledRect(
            ui,
            {rect.x, rect.y + 6.0f, rect.width, 6.0f},
            {0.10f, 0.13f, 0.18f}
        );

        const float u = static_cast<float>(
            std::clamp((value - 5.0) / 45.0, 0.0, 1.0)
        );
        appendFilledRect(
            ui,
            {rect.x, rect.y + 6.0f, rect.width * u, 6.0f},
            {0.24f, 0.64f, 1.0f}
        );

        const float knobX = rect.x + rect.width * u;
        appendFilledRect(
            ui,
            {knobX - 4.0f, rect.y + 1.0f, 8.0f, 16.0f},
            {0.92f, 0.95f, 1.0f}
        );
    };

    appendSpeedSlider(
        startSpeedSliderRect(),
        "СТАРТ V",
        state.startSpeedMps
    );
    appendSpeedSlider(
        finishSpeedSliderRect(),
        "ФИНИШ V",
        state.finishSpeedMps
    );

    if (!state.shortCalculationLog.empty())
    {
        const UiRect shortLogRect {
            16.0f,
            174.0f,
            620.0f,
            26.0f +
                18.0f *
                static_cast<float>(state.shortCalculationLog.size())
        };
        appendFilledRect(
            ui,
            shortLogRect,
            {0.025f, 0.033f, 0.045f}
        );

        float shortY = shortLogRect.y + 10.0f;
        for (const auto& line : state.shortCalculationLog)
        {
            appendUiText(
                ui,
                shortLogRect.x + 12.0f,
                shortY,
                line,
                1.10f,
                recalculationRequired(state)
                    ? glm::vec3(1.0f, 0.78f, 0.28f)
                    : glm::vec3(0.78f, 0.90f, 0.82f)
            );
            shortY += 18.0f;
        }
    }

    // Diagnostics are deliberately below the top controls so resizing or
    // changing a status line can never cover/reflow the controls.
    const float panelWidth = 470.0f;
    const float panelTop = 122.0f;
    const float panelX =
        std::max(
            0.0f,
            static_cast<float>(windowWidth) - panelWidth
        );
    const float panelHeight =
        std::max(
            0.0f,
            static_cast<float>(windowHeight) - panelTop
        );

    appendFilledRect(
        ui,
        {panelX, panelTop, panelWidth, panelHeight},
        {0.045f, 0.055f, 0.070f}
    );

    const float x = panelX + 18.0f;
    appendUiText(
        ui, x, panelTop + 18.0f,
        std::string("НАВИГАЦИЯ 3D — ДИАГНОСТИКА | REV ") +
            kViewerRevision,
        1.55f,
        {0.95f, 0.96f, 1.0f}
    );

    const std::string stageTitle =
        state.executionPerformed
            ? (
                state.executionSucceeded
                    ? "ЭТАП 2: ПОЛЁТ ВЫПОЛНЕН"
                    : "ЭТАП 2: ОШИБКА ИСПОЛНЕНИЯ"
              )
            : hasCalculation
                ? "ЭТАП 1: МАРШРУТ РАССЧИТАН"
                : "СЦЕНА ДО РАСЧЁТА";

    appendUiText(
        ui, x, panelTop + 48.0f,
        stageTitle,
        1.35f,
        (
            state.executionPerformed
                ? state.executionSucceeded
                : (hasCalculation && state.calculationSucceeded)
        )
            ? glm::vec3(0.35f,1.0f,0.42f)
            : glm::vec3(1.0f,0.82f,0.32f)
    );

    appendUiText(
        ui, x, panelTop + 68.0f,
        "ФАЗА: " + localizedPhase(frame.phase),
        1.20f,
        {0.85f,0.87f,0.92f}
    );
    appendUiText(
        ui, x, panelTop + 86.0f,
        "СТАТУС: " + localizedStatus(frame.plannerStatus),
        1.20f,
        {0.85f,0.87f,0.92f}
    );

    appendUiText(
        ui, x, panelTop + 116.0f,
        "ЦЕПОЧКА",
        1.45f,
        {1.0f,0.82f,0.32f}
    );

    float logY = panelTop + 142.0f;
    const std::size_t maxLines = 18;
    for (
        std::size_t i = 0;
        i < state.diagnosticLines.size() && i < maxLines;
        ++i)
    {
        const std::string& line = state.diagnosticLines[i];
        glm::vec3 color(0.86f,0.88f,0.92f);

        if (
            line.find("PLANNER: OK") != std::string::npos ||
            line.find("TRAJECTORY: RUCKIG OK") != std::string::npos ||
            line.find("FOLLOWER: EXECUTED") != std::string::npos ||
            line.find("PILOT BRIDGE: EXECUTED") != std::string::npos)
        {
            color = {0.35f,1.0f,0.42f};
        }
        else if (
            line.find("FAIL") != std::string::npos ||
            line.find("CONTACT: YES") != std::string::npos)
        {
            color = {1.0f,0.30f,0.22f};
        }
        else if (
            line.find("NOT RUN") != std::string::npos ||
            line.find("NOT ENABLED") != std::string::npos)
        {
            color = {1.0f,0.72f,0.25f};
        }

        appendUiText(
            ui,
            x,
            logY,
            line,
            1.10f,
            color
        );
        logY += 18.0f;
    }

    appendUiText(
        ui, x, panelTop + 482.0f,
        "ЧТО ПРОИСХОДИТ",
        1.40f,
        {1.0f,0.82f,0.32f}
    );
    appendUiText(
        ui,
        x,
        panelTop + 506.0f,
        currentExplanation(frame),
        1.10f,
        {0.94f,0.95f,0.97f}
    );

    const float legendWidth = 438.0f;
    const float legendHeight = 226.0f;
    const float legendX =
        std::max(
            8.0f,
            static_cast<float>(windowWidth) - legendWidth - 16.0f
        );
    const float legendY =
        std::max(
            panelTop + 360.0f,
            static_cast<float>(windowHeight) - legendHeight - 54.0f
        );

    appendFilledRect(
        ui,
        {legendX, legendY, legendWidth, legendHeight},
        {0.025f, 0.033f, 0.045f}
    );

    std::ostringstream speedText;
    speedText.setf(std::ios::fixed);
    speedText.precision(1);
    speedText
        << "СКОРОСТЬ: "
        << glm::length(frame.shipVelocity)
        << " М/С"
        << " | MAIN: "
        << std::lround(
            std::clamp(frame.mainEngineThrottle01, 0.0, 1.0) *
            100.0
        )
        << "%";

    appendUiText(
        ui,
        legendX + 14.0f,
        legendY + 12.0f,
        speedText.str(),
        1.45f,
        {1.0f, 0.86f, 0.34f}
    );

    if (frame.hasProgramReference)
    {
        const double referenceSpeed =
            glm::length(frame.programReferenceVelocity);
        const double halfWidth =
            std::max(
                0.0,
                frame.programSpeedCorridorHalfWidthMps
            );

        std::ostringstream corridorText;
        corridorText.setf(std::ios::fixed);
        corridorText.precision(1);
        corridorText
            << "КОРИДОР V: "
            << std::max(0.0, referenceSpeed - halfWidth)
            << " .. "
            << (referenceSpeed + halfWidth)
            << " М/С";

        appendUiText(
            ui,
            legendX + 14.0f,
            legendY + 32.0f,
            corridorText.str(),
            1.10f,
            {0.70f, 0.88f, 1.0f}
        );
    }

    appendUiText(
        ui, legendX + 14.0f, legendY + 58.0f,
        "ЖЁЛТАЯ СТРЕЛКА: ФАКТИЧЕСКИЙ ВЕКТОР СКОРОСТИ",
        1.00f,
        {1.0f, 0.78f, 0.10f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 76.0f,
        "ГОЛУБАЯ КОРОТКАЯ: ФАКТИЧЕСКИЙ НОС КОРАБЛЯ",
        1.00f,
        {0.25f, 0.85f, 1.0f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 94.0f,
        "КРАСНАЯ: ЦЕЛЕВОЙ НОС ПРОГРАММЫ FOLLOWER",
        1.00f,
        {1.0f, 0.25f, 0.18f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 118.0f,
        "БЕЛАЯ: ГРУБЫЙ ГЕОМЕТРИЧЕСКИЙ МАРШРУТ",
        1.00f,
        {0.90f, 0.90f, 0.90f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 136.0f,
        "СИНЯЯ: РАЗДВИНУТЫЙ EXECUTION GUIDE",
        1.00f,
        {0.20f, 0.72f, 1.0f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 154.0f,
        "ФИОЛЕТОВАЯ: РАСЧЁТНАЯ КРИВАЯ RUCKIG",
        1.00f,
        {0.88f, 0.35f, 1.0f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 172.0f,
        "ЗЕЛЁНАЯ: ФАКТИЧЕСКАЯ ТРАЕКТОРИЯ",
        1.00f,
        {0.25f, 1.0f, 0.35f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 190.0f,
        "СЕРЫЙ КАРКАС: СТАТИЧЕСКОЕ ПРЕПЯТСТВИЕ",
        1.00f,
        {0.75f, 0.78f, 0.82f}
    );
    appendUiText(
        ui, legendX + 14.0f, legendY + 208.0f,
        "ОРАНЖЕВАЯ КОРМА: РАБОТАЕТ ГЛАВНЫЙ ДВИГАТЕЛЬ",
        1.00f,
        {1.0f, 0.62f, 0.12f}
    );

    if (hasExecution)
    {
        const UiRect slider =
            frameSliderRect(windowWidth, windowHeight);
        appendFilledRect(
            ui,
            slider,
            {0.10f, 0.12f, 0.16f}
        );

        const float progress =
            data.frames.size() <= 1
                ? 0.0f
                : static_cast<float>(state.frameIndex) /
                  static_cast<float>(data.frames.size() - 1);

        appendFilledRect(
            ui,
            {
                slider.x,
                slider.y,
                slider.width * progress,
                slider.height
            },
            {0.25f, 0.65f, 1.0f}
        );

        const float knobX =
            slider.x + slider.width * progress;
        appendFilledRect(
            ui,
            {
                knobX - 3.0f,
                slider.y - 4.0f,
                6.0f,
                slider.height + 8.0f
            },
            {0.95f, 0.96f, 1.0f}
        );

        appendHorizonInset(ui, frame, windowHeight);
    }

    renderer.draw(GL_TRIANGLES, ui, 1.0f);
    glEnable(GL_DEPTH_TEST);
}


void fitCamera(
    AppState& state,
    const trace::TraceDocument& data
)
{
    glm::vec3 minimum(
        std::numeric_limits<float>::infinity()
    );
    glm::vec3 maximum(
        -std::numeric_limits<float>::infinity()
    );

    auto include = [&](const glm::dvec3& p)
    {
        const glm::vec3 v = toVec3(p);
        minimum = glm::min(minimum, v);
        maximum = glm::max(maximum, v);
    };

    if (data.hasSceneEndpoints)
    {
        include(data.sceneStartMapMeters);
        include(data.sceneFinishMapMeters);
    }

    for (const auto& p : data.routePoints)
        include(p);
    for (const auto& p : data.executionGuidePoints)
        include(p);
    for (const auto& p : data.calculatedTrajectoryPoints)
        include(p);

    for (const auto& obstacle : data.staticObstacles)
    {
        glm::dvec3 half(0.0);

        if (obstacle.shape == "box")
        {
            half = obstacle.halfExtents;
        }
        else if (obstacle.shape == "capsule")
        {
            half = {
                obstacle.radiusMeters,
                obstacle.radiusMeters,
                obstacle.radiusMeters +
                    obstacle.capsuleHalfLengthMeters
            };
        }
        else
        {
            half = glm::dvec3(obstacle.radiusMeters);
        }

        include(obstacle.center - half);
        include(obstacle.center + half);
    }

    for (const auto& f : data.frames)
    {
        include(f.shipPosition);
        if (f.hasSelectedTarget)
            include(f.selectedTarget);
        if (f.hasReacquisitionTarget)
            include(f.reacquisitionTarget);
        if (f.hasPortalTarget)
            include(f.portalTarget);

        if (f.hazardActive)
        {
            include(
                f.hazardPosition +
                glm::dvec3(f.hazardPlannerEnvelopeRadiusMeters)
            );
            include(
                f.hazardPosition -
                glm::dvec3(f.hazardPlannerEnvelopeRadiusMeters)
            );
        }
    }

    if (!std::isfinite(minimum.x) || !std::isfinite(maximum.x))
        return;

    state.camera.target = 0.5f * (minimum + maximum);
    const float radius =
        std::max(20.0f, 0.5f * glm::length(maximum - minimum));
    state.camera.distance = radius * 2.6f;
}

void errorCallback(int code, const char* description)
{
    std::cerr
        << "[navigation_runtime_viewer][GLFW] "
        << code << ": "
        << (description ? description : "<null>")
        << "\n";
}

void setFrameFromSlider(
    AppState& state,
    const trace::TraceDocument& data,
    const UiRect& slider,
    double mouseX
)
{
    if (data.frames.empty() || slider.width <= 1.0f)
        return;

    const double normalized =
        std::clamp(
            (mouseX - slider.x) /
                static_cast<double>(slider.width),
            0.0,
            1.0
        );

    const std::size_t index =
        static_cast<std::size_t>(
            std::llround(
                normalized *
                static_cast<double>(data.frames.size() - 1)
            )
        );

    state.playing = false;
    state.frameIndex =
        std::min(index, data.frames.size() - 1);
    state.playbackTime =
        data.frames[state.frameIndex].timeSeconds -
        data.frames.front().timeSeconds;
}

bool recalculationRequired(const AppState& state)
{
    return
        state.routeInputsDirty ||
        state.executionInputsDirty;
}

void markCalculationDirty(
    AppState& state,
    bool routeAffecting
)
{
    state.executionInputsDirty = true;
    state.routeInputsDirty =
        state.routeInputsDirty || routeAffecting;
    state.playing = false;
    state.calculationMessage =
        routeAffecting
            ? "ИЗМЕНЕНЫ ПАРАМЕТРЫ МАРШРУТА — НУЖЕН ПЕРЕСЧЕТ"
            : "ИЗМЕНЕНЫ ПАРАМЕТРЫ ПОЛЕТА — НУЖЕН ПЕРЕСЧЕТ";
    state.shortCalculationLog = {
        "РЕЗУЛЬТАТ УСТАРЕЛ",
        routeAffecting
            ? "БУДЕТ ПЕРЕСЧИТАН МАРШРУТ И ПОЛЕТ"
            : "МАРШРУТ СОХРАНЕН, БУДЕТ ПЕРЕСЧИТАН ПОЛЕТ"
    };
}

const char* traceLawName(
    elite::tools::navigation_runtime::ControlMode mode
)
{
    return
        mode == elite::tools::navigation_runtime::ControlMode::Assisted
            ? "assisted"
            : "newtonian";
}

void synchronizeTraceLawLabel(
    AppState& state
)
{
    if (state.traceData)
        state.traceData->law = traceLawName(state.controlMode);
    if (state.hasRetainedRoute)
        state.retainedRoute.law = traceLawName(state.controlMode);
}

void reduceViewerState(
    AppState& state,
    const ViewerAction& action
)
{
    switch (action.type)
    {
        case ViewerActionType::SetControlMode:
            if (state.controlMode != action.controlMode)
            {
                state.controlMode = action.controlMode;
                markCalculationDirty(state, false);
                synchronizeTraceLawLabel(state);
            }
            break;

        case ViewerActionType::SetPilot:
            if (state.pilot != action.pilot)
            {
                state.pilot = action.pilot;
                markCalculationDirty(state, false);
            }
            break;

        case ViewerActionType::SetFlightStyle:
            if (state.flightStyle != action.flightStyle)
            {
                state.flightStyle = action.flightStyle;
                markCalculationDirty(state, true);
            }
            break;

        case ViewerActionType::SetStartSpeed:
        {
            const double next =
                std::clamp(action.speedMps, 5.0, 50.0);
            if (std::abs(state.startSpeedMps - next) > 1.0e-6)
            {
                state.startSpeedMps = next;
                markCalculationDirty(state, true);
            }
            break;
        }

        case ViewerActionType::SetFinishSpeed:
        {
            const double next =
                std::clamp(action.speedMps, 5.0, 50.0);
            if (std::abs(state.finishSpeedMps - next) > 1.0e-6)
            {
                state.finishSpeedMps = next;
                markCalculationDirty(state, true);
            }
            break;
        }

        case ViewerActionType::ToggleSuddenObstacle:
            state.useSuddenObstacle = !state.useSuddenObstacle;
            markCalculationDirty(state, false);
            break;

        case ViewerActionType::QueueUiAction:
            state.pendingUiAction = action.uiAction;
            break;

        case ViewerActionType::SetPlaying:
            state.playing = action.boolValue;
            break;

        case ViewerActionType::RequestFit:
            state.requestFit = true;
            break;

        case ViewerActionType::RuntimeControlLawObserved:
        {
            using ControlMode =
                elite::tools::navigation_runtime::ControlMode;

            if (action.runtimeControlLaw == "ASSISTED" ||
                action.runtimeControlLaw == "assisted")
            {
                // Runtime observation is authoritative. Do not restore the
                // retained route here: this action reports what is actually
                // flying, it is not a user's new execution request.
                state.controlMode = ControlMode::Assisted;
                synchronizeTraceLawLabel(state);
            }
            else if (action.runtimeControlLaw == "NEWTONIAN" ||
                     action.runtimeControlLaw == "newtonian")
            {
                state.controlMode = ControlMode::Newtonian;
                synchronizeTraceLawLabel(state);
            }
            break;
        }
    }
}

void dispatchViewerAction(
    AppState& state,
    const ViewerAction& action
)
{
    reduceViewerState(state, action);
}

void queueUiAction(
    AppState& state,
    UiAction uiAction
)
{
    ViewerAction command;
    command.type = ViewerActionType::QueueUiAction;
    command.uiAction = uiAction;
    dispatchViewerAction(state, command);
}

void syncViewerStateFromRuntimeFrame(
    AppState& state,
    const trace::TraceFrame& frame
)
{
    if (!frame.hasRuntimeControlLaw ||
        frame.runtimeControlLaw.empty())
    {
        return;
    }

    ViewerAction action;
    action.type = ViewerActionType::RuntimeControlLawObserved;
    action.runtimeControlLaw = frame.runtimeControlLaw;
    dispatchViewerAction(state, action);
}

void mouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int
)
{
    auto* state =
        static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state)
        return;

    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        state->orbiting = action == GLFW_PRESS;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        state->panning = action == GLFW_PRESS;

    glfwGetCursorPos(window, &state->lastMouseX, &state->lastMouseY);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        const double x = state->lastMouseX;
        const double y = state->lastMouseY;

        if (assistedRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetControlMode;
            change.controlMode =
                elite::tools::navigation_runtime::ControlMode::Assisted;
            dispatchViewerAction(*state, change);
        }
        else if (newtonianRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetControlMode;
            change.controlMode =
                elite::tools::navigation_runtime::ControlMode::Newtonian;
            dispatchViewerAction(*state, change);
        }
        else if (expertRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetPilot;
            change.pilot =
                elite::tools::navigation_runtime::PilotLevel::Expert;
            dispatchViewerAction(*state, change);
        }
        else if (averageRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetPilot;
            change.pilot =
                elite::tools::navigation_runtime::PilotLevel::Average;
            dispatchViewerAction(*state, change);
        }
        else if (loserRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetPilot;
            change.pilot =
                elite::tools::navigation_runtime::PilotLevel::Loser;
            dispatchViewerAction(*state, change);
        }
        else if (standardRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetFlightStyle;
            change.flightStyle =
                elite::tools::navigation_runtime::FlightStyle::Standard;
            dispatchViewerAction(*state, change);
        }
        else if (extremeRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::SetFlightStyle;
            change.flightStyle =
                elite::tools::navigation_runtime::FlightStyle::Extreme;
            dispatchViewerAction(*state, change);
        }
        else if (suddenObstacleRect().contains(x, y))
        {
            ViewerAction change;
            change.type = ViewerActionType::ToggleSuddenObstacle;
            dispatchViewerAction(*state, change);
        }
        else if (startSpeedSliderRect().contains(x, y))
        {
            state->speedSliderDrag = SpeedSliderDrag::Start;
            ViewerAction change;
            change.type = ViewerActionType::SetStartSpeed;
            change.speedMps =
                speedFromSliderX(startSpeedSliderRect(), x);
            dispatchViewerAction(*state, change);
        }
        else if (finishSpeedSliderRect().contains(x, y))
        {
            state->speedSliderDrag = SpeedSliderDrag::Finish;
            ViewerAction change;
            change.type = ViewerActionType::SetFinishSpeed;
            change.speedMps =
                speedFromSliderX(finishSpeedSliderRect(), x);
            dispatchViewerAction(*state, change);
        }
        else if (
            calculateButtonRect().contains(x, y) &&
            recalculationRequired(*state) &&
            !state->calculationInProgress)
        {
            queueUiAction(*state, UiAction::Calculate);
        }
        else if (
            playButtonRect().contains(x, y) &&
            state->traceData &&
            state->traceData->frames.size() > 1)
        {
            queueUiAction(*state, UiAction::TogglePlay);
        }
        else if (
            prevButtonRect().contains(x, y) &&
            state->traceData &&
            state->traceData->frames.size() > 1)
        {
            queueUiAction(*state, UiAction::PreviousFrame);
        }
        else if (
            nextButtonRect().contains(x, y) &&
            state->traceData &&
            state->traceData->frames.size() > 1)
        {
            queueUiAction(*state, UiAction::NextFrame);
        }
        else if (
            replanButtonRect().contains(x, y) &&
            state->traceData &&
            state->traceData->frames.size() > 1)
        {
            queueUiAction(*state, UiAction::NextReplan);
        }
        else if (fitButtonRect().contains(x, y))
        {
            queueUiAction(*state, UiAction::Fit);
        }
        else if (state->traceData)
        {
            int width = 1;
            int height = 1;
            glfwGetWindowSize(window, &width, &height);
            const UiRect slider = frameSliderRect(width, height);
            if (slider.contains(x, y))
            {
                state->scrubbingFrames = true;
                setFrameFromSlider(
                    *state,
                    *state->traceData,
                    slider,
                    x
                );
            }
        }
    }
    else if (
        button == GLFW_MOUSE_BUTTON_LEFT &&
        action == GLFW_RELEASE)
    {
        state->scrubbingFrames = false;
        const bool speedChanged =
            state->speedSliderDrag != SpeedSliderDrag::None;
        state->speedSliderDrag = SpeedSliderDrag::None;
        if (speedChanged)
    }
}

void cursorCallback(GLFWwindow* window, double x, double y)
{
    auto* state =
        static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state)
        return;

    const double dx = x - state->lastMouseX;
    const double dy = y - state->lastMouseY;
    state->lastMouseX = x;
    state->lastMouseY = y;

    if (state->scrubbingFrames && state->traceData)
    {
        int width = 1;
        int height = 1;
        glfwGetWindowSize(window, &width, &height);
        setFrameFromSlider(
            *state,
            *state->traceData,
            frameSliderRect(width, height),
            x
        );
    }

    if (state->speedSliderDrag == SpeedSliderDrag::Start)
    {
        ViewerAction change;
        change.type = ViewerActionType::SetStartSpeed;
        change.speedMps =
            speedFromSliderX(startSpeedSliderRect(), x);
        dispatchViewerAction(*state, change);
    }
    else if (state->speedSliderDrag == SpeedSliderDrag::Finish)
    {
        ViewerAction change;
        change.type = ViewerActionType::SetFinishSpeed;
        change.speedMps =
            speedFromSliderX(finishSpeedSliderRect(), x);
        dispatchViewerAction(*state, change);
    }

    if (state->orbiting)
    {
        state->camera.yaw += static_cast<float>(dx * 0.006);
        state->camera.pitch =
            std::clamp(
                state->camera.pitch -
                    static_cast<float>(dy * 0.006),
                -1.45f,
                1.45f
            );
    }

    if (state->panning)
    {
        const glm::vec3 eye = state->camera.position();
        const glm::vec3 forward =
            glm::normalize(state->camera.target - eye);
        const glm::vec3 right =
            glm::normalize(
                glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f))
            );
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        const float scale = state->camera.distance * 0.0016f;
        state->camera.target +=
            right * static_cast<float>(-dx) * scale +
            up * static_cast<float>(dy) * scale;
    }
}

void scrollCallback(GLFWwindow* window, double, double yOffset)
{
    auto* state =
        static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state)
        return;

    state->camera.distance *=
        std::pow(0.86f, static_cast<float>(yOffset));
    state->camera.distance =
        std::clamp(state->camera.distance, 5.0f, 10000.0f);
}

void keyCallback(
    GLFWwindow* window,
    int key,
    int,
    int action,
    int
)
{
    if (action != GLFW_PRESS)
        return;

    auto* state =
        static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state)
        return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if (
        key == GLFW_KEY_SPACE &&
        state->traceData &&
        state->traceData->frames.size() > 1)
    {
        ViewerAction play;
        play.type = ViewerActionType::SetPlaying;
        play.boolValue = !state->playing;
        dispatchViewerAction(*state, play);
    }
    else if (key == GLFW_KEY_F)
    {
        ViewerAction fit;
        fit.type = ViewerActionType::RequestFit;
        dispatchViewerAction(*state, fit);
    }
}

void drawScene(
    PrimitiveRenderer& renderer,
    const trace::TraceDocument& data,
    std::size_t frameIndex,
    const trace::TraceFrame& displayFrame,
    const glm::mat4& viewProjection
)
{
    if (data.frames.empty())
        return;

    frameIndex = std::min(frameIndex, data.frames.size() - 1);
    const trace::TraceFrame& frame = displayFrame;

    renderer.begin(viewProjection);

    if (data.hasSceneEndpoints)
    {
        std::vector<Vertex> grid;
        appendReferenceGrid(
            grid,
            data.sceneStartMapMeters,
            data.sceneFinishMapMeters
        );
        renderer.draw(GL_LINES, grid, 1.0f);

        std::vector<Vertex> endpoints;
        appendCross(
            endpoints,
            toVec3(data.sceneStartMapMeters),
            8.0f,
            {0.25f, 1.0f, 0.35f}
        );
        appendCross(
            endpoints,
            toVec3(data.sceneFinishMapMeters),
            9.0f,
            {1.0f, 0.92f, 0.15f}
        );
        appendCircle(
            endpoints,
            toVec3(data.sceneFinishMapMeters),
            12.0f,
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.92f, 0.15f},
            40
        );
        renderer.draw(GL_LINES, endpoints, 3.0f);
    }

    std::vector<Vertex> staticGeometry;
    for (const auto& obstacle : data.staticObstacles)
    {
        const glm::vec3 center = toVec3(obstacle.center);
        const glm::vec3 color(0.60f, 0.64f, 0.70f);

        if (obstacle.shape == "box")
        {
            appendWireBox(
                staticGeometry,
                center,
                toVec3(obstacle.halfExtents),
                color
            );
        }
        else if (obstacle.shape == "capsule")
        {
            appendWireCapsuleZ(
                staticGeometry,
                center,
                static_cast<float>(obstacle.radiusMeters),
                static_cast<float>(
                    obstacle.capsuleHalfLengthMeters
                ),
                color
            );
        }
        else
        {
            appendWireSphere(
                staticGeometry,
                center,
                static_cast<float>(obstacle.radiusMeters),
                color
            );
        }
    }
    renderer.draw(GL_LINES, staticGeometry, 1.5f);

    renderer.draw(
        GL_LINES,
        polyline(
            data.routePoints,
            glm::vec3(0.88f, 0.88f, 0.88f)
        ),
        2.0f
    );

    // Stage-2 local guide: entry/exit support geometry derived from the
    // retained coarse route. This line shows whether the corner was widened
    // before Ruckig solved the actual continuous reference.
    renderer.draw(
        GL_LINES,
        polyline(
            data.executionGuidePoints,
            glm::vec3(0.20f, 0.72f, 1.0f)
        ),
        1.5f
    );

    // Full collision-checked continuous reference handed to Follower. This is
    // the curve the user needs to compare against the green flown trajectory.
    renderer.draw(
        GL_LINES,
        polyline(
            data.calculatedTrajectoryPoints,
            glm::vec3(0.88f, 0.35f, 1.0f)
        ),
        3.0f
    );

    // The planner result itself does not publish a volumetric corridor.
    // What execution owns is the accepted maneuver reference plus its follower
    // position-error envelope. Render that product explicitly and label it as
    // the maneuver-program tracking corridor.
    if (data.frames[frameIndex].hasProgramReference)
    {
        std::size_t first = frameIndex;
        while (
            first > 0 &&
            data.frames[first - 1].hasProgramReference &&
            data.frames[first - 1].phase == data.frames[frameIndex].phase
        )
        {
            --first;
        }

        std::size_t last = frameIndex;
        while (
            last + 1 < data.frames.size() &&
            data.frames[last + 1].hasProgramReference &&
            data.frames[last + 1].phase == data.frames[frameIndex].phase
        )
        {
            ++last;
        }

        std::vector<glm::dvec3> referencePath;
        referencePath.reserve(last - first + 1);
        for (std::size_t i = first; i <= last; ++i)
            referencePath.push_back(data.frames[i].programReferencePosition);

        std::vector<Vertex> corridorTriangles;
        appendTrackingTube(
            corridorTriangles,
            referencePath,
            static_cast<float>(
                data.frames[frameIndex].
                    programTrackingCorridorRadiusMeters
            ),
            {0.20f, 0.55f, 1.0f},
            0.16f
        );

        glDepthMask(GL_FALSE);
        renderer.draw(GL_TRIANGLES, corridorTriangles, 1.0f);
        glDepthMask(GL_TRUE);
    }

    std::vector<Vertex> markers;
    for (const auto& p : data.turnPoints)
        appendCross(markers, toVec3(p), 3.0f, {1.0f, 0.65f, 0.15f});
    renderer.draw(GL_LINES, markers, 2.0f);


    std::vector<glm::dvec3> actualPath;
    actualPath.reserve(frameIndex + 1);
    for (std::size_t i = 0; i <= frameIndex; ++i)
        actualPath.push_back(data.frames[i].shipPosition);
    renderer.draw(
        GL_LINES,
        polyline(actualPath, {0.25f, 1.0f, 0.35f}),
        2.0f
    );

    std::vector<glm::dvec3> hazardPath;
    for (std::size_t i = 0; i <= frameIndex; ++i)
    {
        if (data.frames[i].hazardActive)
            hazardPath.push_back(data.frames[i].hazardPosition);
    }
    renderer.draw(
        GL_LINES,
        polyline(hazardPath, {1.0f, 0.25f, 0.20f}),
        1.5f
    );

    std::vector<Vertex> replanMarkers;
    for (std::size_t i = 0; i <= frameIndex; ++i)
    {
        if (data.frames[i].replanEvent)
        {
            appendCross(
                replanMarkers,
                toVec3(data.frames[i].shipPosition),
                4.5f,
                {1.0f, 0.45f, 0.05f}
            );
        }
    }
    renderer.draw(GL_LINES, replanMarkers, 3.0f);

    if (frame.hazardActive)
    {
        std::vector<Vertex> hazard;
        appendWireSphere(
            hazard,
            toVec3(frame.hazardPosition),
            static_cast<float>(frame.hazardRadiusMeters),
            {1.0f, 0.18f, 0.18f}
        );
        appendWireSphere(
            hazard,
            toVec3(frame.hazardPosition),
            static_cast<float>(
                frame.hazardCollisionEnvelopeRadiusMeters
            ),
            {0.95f, 0.32f, 0.22f}
        );
        appendWireSphere(
            hazard,
            toVec3(frame.hazardPosition),
            static_cast<float>(
                frame.hazardPlannerEnvelopeRadiusMeters
            ),
            {0.75f, 0.25f, 0.20f}
        );
        renderer.draw(GL_LINES, hazard, 1.5f);
    }

    std::vector<Vertex> currentMarkers;
    if (frame.hasSelectedTarget)
        appendCross(
            currentMarkers,
            toVec3(frame.selectedTarget),
            4.0f,
            {1.0f, 0.92f, 0.15f}
        );
    if (frame.hasReacquisitionTarget)
        appendCross(
            currentMarkers,
            toVec3(frame.reacquisitionTarget),
            4.0f,
            {0.20f, 0.95f, 1.0f}
        );
    if (frame.hasPortalTarget)
        appendCross(
            currentMarkers,
            toVec3(frame.portalTarget),
            5.0f,
            {0.85f, 0.30f, 1.0f}
        );
    renderer.draw(GL_LINES, currentMarkers, 3.0f);

    std::vector<Vertex> mainEngineFace;
    appendMainEngineThrustFace(
        mainEngineFace,
        frame,
        data.shipHalfExtentsMeters
    );
    renderer.draw(GL_TRIANGLES, mainEngineFace, 1.0f);

    std::vector<Vertex> ship;
    appendShipBoxAndArrow(
        ship,
        frame,
        data.shipHalfExtentsMeters
    );
    appendReferenceOrientationArrow(
        ship,
        frame,
        data.shipHalfExtentsMeters
    );
    renderer.draw(GL_LINES, ship, 2.0f);

    std::vector<Vertex> velocityVector;
    appendVelocityVector(velocityVector, frame);
    renderer.draw(GL_LINES, velocityVector, 6.0f);
}

void setWindowTitle(
    GLFWwindow* window,
    const trace::TraceDocument& data,
    std::size_t frameIndex
)
{
    if (data.frames.empty())
        return;

    const auto& f =
        data.frames[std::min(frameIndex, data.frames.size() - 1)];

    if (f.phase == "scene_preview")
    {
        const std::string previewTitle =
            std::string("Навигация 3D | REV ") +
            kViewerRevision +
            " - СЦЕНА ДО РАСЧЁТА";
        glfwSetWindowTitle(
            window,
            previewTitle.c_str()
        );
        return;
    }

    std::ostringstream title;
    title.setf(std::ios::fixed);
    title.precision(2);
    title
        << "Навигация 3D | REV "
        << kViewerRevision
        << " - "
        << localizedLaw(
            f.hasRuntimeControlLaw
                ? f.runtimeControlLaw
                : data.law
        )
        << " | кадр " << (frameIndex + 1)
        << "/" << data.frames.size()
        << " | t=" << f.timeSeconds << " с"
        << " | " << localizedPhase(f.phase);

    if (!f.plannerStatus.empty())
        title << " | " << f.plannerStatus;

    if (f.hazardActive)
        title << " | зазор=" << f.dynamicClearanceMeters << " м";

    if (f.replanEvent)
        title << " | ПЕРЕПЛАНИРОВАНИЕ";

    glfwSetWindowTitle(window, title.str().c_str());
}

void advanceManualFrame(
    AppState& state,
    const trace::TraceDocument& data,
    int delta
)
{
    if (data.frames.empty())
        return;

    state.playing = false;
    const long current = static_cast<long>(state.frameIndex);
    const long last =
        static_cast<long>(data.frames.size() - 1);
    const long next =
        std::clamp(current + static_cast<long>(delta), 0L, last);

    state.frameIndex = static_cast<std::size_t>(next);
    state.playbackTime =
        data.frames[state.frameIndex].timeSeconds -
        data.frames.front().timeSeconds;
}

void jumpToNextReplan(
    AppState& state,
    const trace::TraceDocument& data
)
{
    if (data.frames.empty())
        return;

    state.playing = false;
    for (std::size_t i = state.frameIndex + 1;
         i < data.frames.size();
         ++i)
    {
        if (data.frames[i].replanEvent)
        {
            state.frameIndex = i;
            state.playbackTime =
                data.frames[i].timeSeconds -
                data.frames.front().timeSeconds;
            return;
        }
    }

    for (std::size_t i = 0; i <= state.frameIndex; ++i)
    {
        if (data.frames[i].replanEvent)
        {
            state.frameIndex = i;
            state.playbackTime =
                data.frames[i].timeSeconds -
                data.frames.front().timeSeconds;
            return;
        }
    }
}


void processUiAction(
    AppState& state,
    trace::TraceDocument& data
)
{
    const UiAction action = state.pendingUiAction;
    state.pendingUiAction = UiAction::None;

    switch (action)
    {
        case UiAction::Calculate:
        {
            if (!recalculationRequired(state) ||
                state.calculationInProgress)
            {
                break;
            }

            state.calculationInProgress = true;
            state.calculationPerformed = true;
            state.calculationSucceeded = false;
            state.executionPerformed = false;
            state.executionSucceeded = false;
            state.calculationMessage = "ИДЕТ РАСЧЕТ...";
            state.shortCalculationLog = {
                "РАСЧЕТ: ВЫПОЛНЯЕТСЯ"
            };
            state.playing = false;
            state.frameIndex = 0;
            state.playbackTime = 0.0;

            elite::tools::navigation_runtime::ScenarioRunSettings settings;
            settings.controlMode = state.controlMode;
            settings.pilot = state.pilot;
            settings.flightStyle = state.flightStyle;
            settings.enableSuddenObstacle =
                state.useSuddenObstacle;
            settings.startSpeedOverrideMps =
                state.startSpeedMps;
            settings.finishSpeedOverrideMps =
                state.finishSpeedMps;

            bool routeReady =
                state.hasRetainedRoute &&
                state.retainedRoute.routePoints.size() >= 2;

            if (state.routeInputsDirty || !routeReady)
            {
                const auto routeResult =
                    elite::tools::navigation_runtime::calculateScenario(
                        state.scenarioPath,
                        settings
                    );

                data = routeResult.trace;
                state.traceData = &data;
                state.frameIndex = 0;
                state.playbackTime = 0.0;
                state.requestFit = !data.frames.empty();
                state.diagnosticLines = routeResult.diagnostics;
                state.calculationSucceeded = routeResult.success;
                state.calculationMessage =
                    routeResult.success
                        ? routeResult.message
                        : "ОШИБКА: " + routeResult.message;

                state.hasRetainedRoute = routeResult.success;
                routeReady = routeResult.success;

                if (routeResult.success)
                {
                    state.retainedRoute = data;
                    state.retainedRouteDiagnostics =
                        routeResult.diagnostics;
                    state.retainedRouteMessage =
                        routeResult.message;
                }
                else
                {
                    state.retainedRoute = trace::TraceDocument {};
                    state.retainedRouteDiagnostics.clear();
                    state.retainedRouteMessage.clear();
                }

                if (!data.frames.empty())
                {
#ifdef ELITE_SOURCE_ROOT
                    const std::string outputPath =
                        std::string(ELITE_SOURCE_ROOT) +
                        "/tools/navigation_runtime/last_calculated_trace.json";
#else
                    const std::string outputPath =
                        "tools/navigation_runtime/last_calculated_trace.json";
#endif
                    try
                    {
                        trace::saveTraceJson(data, outputPath);
                    }
                    catch (const std::exception& e)
                    {
                        state.calculationMessage +=
                            std::string(" | TRACE: ") + e.what();
                    }
                }
            }
            else
            {
                state.calculationSucceeded = true;
            }

            if (routeReady)
            {
                state.executionPerformed = true;

                const auto executionResult =
                    elite::tools::navigation_runtime::executeCalculatedRoute(
                        state.scenarioPath,
                        settings,
                        state.retainedRoute
                    );

                data = executionResult.trace;
                state.traceData = &data;
                state.frameIndex = 0;
                state.playbackTime = 0.0;
                state.executionSucceeded = executionResult.success;
                state.calculationMessage =
                    executionResult.success
                        ? executionResult.message
                        : "ОШИБКА: " + executionResult.message;
                state.diagnosticLines = executionResult.diagnostics;
                state.requestFit = !data.frames.empty();
                state.playing = data.frames.size() > 1;

                if (!data.frames.empty())
                {
#ifdef ELITE_SOURCE_ROOT
                    const std::string outputPath =
                        std::string(ELITE_SOURCE_ROOT) +
                        "/tools/navigation_runtime/last_execution_trace.json";
#else
                    const std::string outputPath =
                        "tools/navigation_runtime/last_execution_trace.json";
#endif
                    try
                    {
                        trace::saveTraceJson(data, outputPath);
                    }
                    catch (const std::exception& e)
                    {
                        state.calculationMessage +=
                            std::string(" | TRACE: ") + e.what();
                    }
                }
            }

            // Same inputs produce the same result. The Calculate button stays
            // disabled after the attempt (success or failure) until a user
            // changes an input that invalidates the result.
            state.routeInputsDirty = false;
            state.executionInputsDirty = false;
            state.calculationInProgress = false;

            auto routeLengthMeters = [](const trace::TraceDocument& route)
            {
                double total = 0.0;
                for (std::size_t i = 1; i < route.routePoints.size(); ++i)
                    total += glm::length(
                        route.routePoints[i] -
                        route.routePoints[i - 1]
                    );
                return total;
            };

            std::ostringstream routeSummary;
            routeSummary.setf(std::ios::fixed);
            routeSummary << std::setprecision(1);
            if (routeReady)
            {
                routeSummary
                    << "МАРШРУТ: "
                    << state.retainedRoute.routePoints.size()
                    << " ТОЧКИ, "
                    << routeLengthMeters(state.retainedRoute)
                    << " М";
            }
            else
            {
                routeSummary << "МАРШРУТ: ОШИБКА";
            }

            std::ostringstream speedSummary;
            speedSummary.setf(std::ios::fixed);
            speedSummary << std::setprecision(1)
                << "V: " << state.startSpeedMps
                << " -> " << state.finishSpeedMps
                << " М/С";

            state.shortCalculationLog = {
                routeReady
                    ? "РАСЧЕТ: ЗАВЕРШЕН"
                    : "РАСЧЕТ: ОШИБКА",
                routeSummary.str(),
                state.executionPerformed
                    ? (
                        state.executionSucceeded
                            ? "ПОЛЕТ: OK"
                            : "ПОЛЕТ: ОШИБКА"
                      )
                    : "ПОЛЕТ: НЕ ЗАПУЩЕН",
                speedSummary.str()
            };
            break;
        }
        case UiAction::TogglePlay:
            if (data.frames.size() > 1)
                state.playing = !state.playing;
            break;
        case UiAction::PreviousFrame:
            advanceManualFrame(state, data, -1);
            break;
        case UiAction::NextFrame:
            advanceManualFrame(state, data, +1);
            break;
        case UiAction::NextReplan:
            jumpToNextReplan(state, data);
            break;
        case UiAction::Fit:
            state.requestFit = true;
            break;
        case UiAction::None:
            break;
    }
}

std::string defaultScenarioPath()
{
#ifdef ELITE_SOURCE_ROOT
    return std::string(ELITE_SOURCE_ROOT) +
        "/tools/navigation_runtime/scenario.json";
#else
    return "tools/navigation_runtime/scenario.json";
#endif
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::string scenarioPath =
            argc >= 2 ? argv[1] : defaultScenarioPath();

        const auto preview =
            elite::tools::navigation_runtime::loadScenarioPreview(
                scenarioPath
            );
        if (!preview.success)
        {
            throw std::runtime_error(
                "scenario preview failed: " + preview.message
            );
        }

        trace::TraceDocument data = preview.trace;

        glfwSetErrorCallback(errorCallback);
        if (!glfwInit())
            throw std::runtime_error("GLFW initialization failed");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        // Ordinary decorated Windows window, maximized to the desktop work area.
        // This is intentionally NOT an exclusive/borderless fullscreen mode.
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

        const std::string initialWindowTitle =
            std::string("Навигация 3D | REV ") +
            kViewerRevision;

        GLFWwindow* window =
            glfwCreateWindow(
                1280,
                800,
                initialWindowTitle.c_str(),
                nullptr,
                nullptr
            );

        if (!window)
        {
            glfwTerminate();
            throw std::runtime_error("could not create viewer window");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        if (gladLoadGL(glfwGetProcAddress) == 0)
        {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error("GLAD initialization failed");
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        AppState state;
        state.traceData = &data;
        state.scenarioPath = scenarioPath;
        state.calculationPerformed = false;
        state.calculationSucceeded = false;
        state.routeInputsDirty = true;
        state.executionInputsDirty = true;
        state.calculationInProgress = false;
        state.calculationMessage = preview.message;
        state.shortCalculationLog = {
            "РАСЧЕТ НЕ ВЫПОЛНЕН",
            "НАЖМИТЕ РАССЧИТАТЬ"
        };
        state.diagnosticLines = preview.diagnostics;
        state.startSpeedMps =
            std::clamp(preview.authoredStartSpeedMps, 5.0, 50.0);
        state.finishSpeedMps =
            std::clamp(preview.authoredFinishSpeedMps, 5.0, 50.0);
        state.playing = false;
        state.requestFit = true;
        state.lastRealTime = glfwGetTime();

        glfwSetWindowUserPointer(window, &state);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);

        PrimitiveRenderer renderer;

        bool leftBracketWasDown = false;
        bool rightBracketWasDown = false;
        bool rWasDown = false;

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            processUiAction(state, data);

            const bool leftDown =
                glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
            const bool rightDown =
                glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
            const bool rDown =
                glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

            if (leftDown && !leftBracketWasDown)
                advanceManualFrame(state, data, -1);
            if (rightDown && !rightBracketWasDown)
                advanceManualFrame(state, data, +1);
            if (rDown && !rWasDown)
                jumpToNextReplan(state, data);

            leftBracketWasDown = leftDown;
            rightBracketWasDown = rightDown;
            rWasDown = rDown;

            if (state.requestFit)
            {
                fitCamera(state, data);
                state.requestFit = false;
            }

            const double now = glfwGetTime();
            const double realDt = now - state.lastRealTime;
            state.lastRealTime = now;

            if (state.playing && data.frames.size() > 1)
            {
                state.playbackTime += realDt;
                const double duration =
                    data.frames.back().timeSeconds -
                    data.frames.front().timeSeconds;

                if (duration > 0.0 && state.playbackTime > duration)
                {
                    state.playbackTime =
                        std::fmod(state.playbackTime, duration);
                    state.frameIndex = 0;
                }

                const double desiredTime =
                    data.frames.front().timeSeconds +
                    state.playbackTime;

                while (
                    state.frameIndex + 1 < data.frames.size() &&
                    data.frames[state.frameIndex + 1].timeSeconds <=
                        desiredTime
                )
                {
                    ++state.frameIndex;
                }
            }

            int width = 1;
            int height = 1;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);

            glClearColor(0.025f, 0.03f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect =
                static_cast<float>(std::max(width, 1)) /
                static_cast<float>(std::max(height, 1));

            const glm::mat4 projection =
                glm::perspective(
                    glm::radians(50.0f),
                    aspect,
                    0.1f,
                    20000.0f
                );

            const trace::TraceFrame displayFrame =
                data.frames.empty()
                    ? trace::TraceFrame {}
                    : interpolatedDisplayFrame(data, state);

            // Runtime truth wins over stale UI selection. If any lower layer
            // ever changes the effective control law, the same Redux-style
            // store that drives the buttons observes it on the displayed frame.
            syncViewerStateFromRuntimeFrame(
                state,
                displayFrame
            );

            drawScene(
                renderer,
                data,
                state.frameIndex,
                displayFrame,
                projection * state.camera.view()
            );

            int windowWidth = 1;
            int windowHeight = 1;
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
            drawHud(
                renderer,
                data,
                state,
                displayFrame,
                windowWidth,
                windowHeight
            );

            setWindowTitle(window, data, state.frameIndex);
            glfwSwapBuffers(window);
        }

        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "NAVIGATION RUNTIME VIEWER: FAIL: "
            << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
