#include "NavigationTrace.h"

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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace trace = elite::tools::navigation_runtime;

namespace
{

constexpr float kPi = 3.14159265358979323846f;

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
    TogglePlay,
    PreviousFrame,
    NextFrame,
    NextReplan,
    Fit
};

struct AppState
{
    Camera camera;
    bool orbiting = false;
    bool panning = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    bool playing = true;
    std::size_t frameIndex = 0;
    double playbackTime = 0.0;
    double lastRealTime = 0.0;

    bool requestFit = true;
    UiAction pendingUiAction = UiAction::None;
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
    const glm::vec3& color
)
{
    out.push_back({a, color});
    out.push_back({b, color});
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
                    right * (static_cast<float>(sx) * hx) +
                    up * (static_cast<float>(sy) * hy) +
                    forward * (static_cast<float>(sz) * hz);
            }
        }
    }

    const glm::vec3 boxColor(0.70f, 0.88f, 0.72f);
    static constexpr int edges[][2] = {
        {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
        {2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
    };
    for (const auto& edge : edges)
        addLine(out, corners[edge[0]], corners[edge[1]], boxColor);

    const glm::vec3 arrowColor(0.25f, 0.85f, 1.0f);
    const glm::vec3 tip =
        center + forward * (hz + std::max(6.0f, hz * 0.75f));
    const glm::vec3 arrowBase = center + forward * hz;
    addLine(out, arrowBase, tip, arrowColor);

    const float wing = std::max(3.0f, hx * 0.40f);
    const glm::vec3 wingBase =
        tip - forward * std::max(4.0f, hz * 0.35f);
    addLine(out, tip, wingBase + right * wing, arrowColor);
    addLine(out, tip, wingBase - right * wing, arrowColor);
    addLine(out, tip, wingBase + up * wing, arrowColor);
    addLine(out, tip, wingBase - up * wing, arrowColor);
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

    const float hz = static_cast<float>(halfExtents.z);
    const glm::vec3 tip =
        center + forward * (hz + std::max(10.0f, hz));
    addLine(
        out,
        center,
        tip,
        {1.0f, 0.25f, 0.95f}
    );
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

    if (a.hazardActive && b.hazardActive)
    {
        out.hazardPosition =
            lerp3(a.hazardPosition, b.hazardPosition);
        out.dynamicClearanceMeters =
            a.dynamicClearanceMeters +
            (b.dynamicClearanceMeters - a.dynamicClearanceMeters) * t;
    }

    if (a.hasProgramReference && b.hasProgramReference)
    {
        out.hasProgramReference = true;
        out.programReferencePosition =
            lerp3(a.programReferencePosition, b.programReferencePosition);
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

UiRect playButtonRect() { return {16.0f, 16.0f, 92.0f, 30.0f}; }
UiRect prevButtonRect() { return {116.0f, 16.0f, 74.0f, 30.0f}; }
UiRect nextButtonRect() { return {198.0f, 16.0f, 74.0f, 30.0f}; }
UiRect replanButtonRect() { return {280.0f, 16.0f, 128.0f, 30.0f}; }
UiRect fitButtonRect() { return {416.0f, 16.0f, 64.0f, 30.0f}; }

std::array<std::uint8_t, 7> glyphRows(char c)
{
    if (c >= 'a' && c <= 'z')
        c = static_cast<char>(c - 'a' + 'A');

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

    for (char ch : text)
    {
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

void appendUiButton(
    std::vector<Vertex>& triangles,
    const UiRect& rect,
    const std::string& label,
    bool active = false
)
{
    appendFilledRect(
        triangles,
        rect,
        active
            ? glm::vec3(0.18f, 0.34f, 0.22f)
            : glm::vec3(0.12f, 0.14f, 0.18f)
    );

    appendUiText(
        triangles,
        rect.x + 9.0f,
        rect.y + 9.0f,
        label,
        1.6f,
        {0.92f, 0.94f, 0.98f}
    );
}

std::string currentExplanation(const trace::TraceFrame& frame)
{
    if (frame.phase == "dynamic_replan")
        return "HAZARD INVALIDATED ACCEPTED ROUTE - LOCAL REPLAN";

    if (frame.phase.rfind("dynamic_bypass_", 0) == 0)
        return "SHIP EXECUTES PHYSICALLY BOUNDED LOCAL BYPASS";

    if (frame.phase.rfind("dynamic_brake_", 0) == 0)
        return "NO SAFE PHYSICAL BYPASS - ACTIVE BRAKING";

    if (frame.phase.rfind("replan_", 0) == 0)
    {
        if (frame.plannerStatus == "adjusted_clear")
            return "REPLAN: HAZARD STILL BLOCKS NOMINAL SEGMENT";
        if (frame.plannerStatus == "nominal_clear")
            return "REPLAN: NEXT BOUNDED SEGMENT LOOKS NOMINAL-CLEAR";
        return "LOCAL REPLAN RESULT";
    }

    if (frame.phase == "portal_101")
        return "FOLLOW STATIC TOPOLOGY ROUTE TO PORTAL 101";

    if (frame.phase == "doctrine_prefix")
        return "EXECUTE ACCEPTED MANEUVER TOWARD PORTAL 102";

    if (frame.phase == "portal_102")
        return "LONG PORTAL LEG - CURRENT KNOWN CLEARANCE-LOSS AREA";

    if (frame.phase == "final_capture")
        return "FINAL PRECISION CAPTURE";

    return "INITIAL ROUTE / TRACE START";
}

void drawHud(
    PrimitiveRenderer& renderer,
    const trace::TraceDocument& data,
    const AppState& state,
    int windowWidth,
    int windowHeight
)
{
    if (data.frames.empty())
        return;

    const auto& frame =
        data.frames[std::min(state.frameIndex, data.frames.size() - 1)];

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
    appendUiButton(
        ui,
        playButtonRect(),
        state.playing ? "PAUSE" : "PLAY",
        state.playing
    );
    appendUiButton(ui, prevButtonRect(), "PREV");
    appendUiButton(ui, nextButtonRect(), "NEXT");
    appendUiButton(ui, replanButtonRect(), "NEXT REPLAN");
    appendUiButton(ui, fitButtonRect(), "FIT");

    const float panelWidth = 372.0f;
    const float panelX =
        std::max(0.0f, static_cast<float>(windowWidth) - panelWidth);
    appendFilledRect(
        ui,
        {
            panelX,
            0.0f,
            panelWidth,
            static_cast<float>(windowHeight)
        },
        {0.045f, 0.055f, 0.070f}
    );

    float x = panelX + 18.0f;
    float y = 20.0f;
    const float textScale = 1.45f;
    const float line = 18.0f;

    appendUiText(
        ui, x, y,
        "NAVIGATION RUNTIME 3D",
        1.65f,
        {0.95f, 0.96f, 1.0f}
    );
    y += 30.0f;

    std::ostringstream frameLine;
    frameLine << "LAW: " << data.law;
    appendUiText(ui, x, y, frameLine.str(), textScale, {0.75f,0.82f,0.92f});
    y += line;

    std::ostringstream indexLine;
    indexLine << "FRAME: " << (state.frameIndex + 1) << "/" << data.frames.size();
    appendUiText(ui, x, y, indexLine.str(), textScale, {0.75f,0.82f,0.92f});
    y += line;

    std::ostringstream timeLine;
    timeLine.setf(std::ios::fixed);
    timeLine.precision(2);
    timeLine << "TIME: " << frame.timeSeconds << " S";
    appendUiText(ui, x, y, timeLine.str(), textScale, {0.75f,0.82f,0.92f});
    y += line;

    appendUiText(ui, x, y, "PHASE: " + frame.phase, textScale, {0.92f,0.92f,0.92f});
    y += line;

    appendUiText(
        ui, x, y,
        "STATUS: " + (frame.plannerStatus.empty() ? std::string("NONE") : frame.plannerStatus),
        textScale,
        {0.92f,0.92f,0.92f}
    );
    y += line;

    if (frame.hazardActive)
    {
        std::ostringstream clearanceLine;
        clearanceLine.setf(std::ios::fixed);
        clearanceLine.precision(2);
        clearanceLine
            << "CLEARANCE: "
            << frame.dynamicClearanceMeters
            << " M";
        appendUiText(
            ui,
            x,
            y,
            clearanceLine.str(),
            textScale,
            frame.dynamicClearanceMeters > 0.5
                ? glm::vec3(0.35f, 1.0f, 0.42f)
                : glm::vec3(1.0f, 0.28f, 0.22f)
        );
        y += line;
    }

    if (frame.replanEvent)
    {
        appendUiText(
            ui, x, y,
            "EVENT: REPLAN",
            textScale,
            {1.0f, 0.55f, 0.10f}
        );
        y += line;
    }

    y += 14.0f;
    appendUiText(ui, x, y, "WHAT IS HAPPENING", 1.55f, {1.0f,0.82f,0.32f});
    y += 23.0f;

    const std::string explanation = currentExplanation(frame);
    const std::size_t splitAt =
        explanation.size() > 34 ? explanation.find(' ', 30) : std::string::npos;

    if (splitAt != std::string::npos)
    {
        appendUiText(
            ui, x, y,
            explanation.substr(0, splitAt),
            1.35f,
            {0.96f,0.96f,0.96f}
        );
        y += line;
        appendUiText(
            ui, x, y,
            explanation.substr(splitAt + 1),
            1.35f,
            {0.96f,0.96f,0.96f}
        );
        y += line;
    }
    else
    {
        appendUiText(ui, x, y, explanation, 1.35f, {0.96f,0.96f,0.96f});
        y += line;
    }

    y += 20.0f;
    appendUiText(ui, x, y, "LEGEND", 1.55f, {0.92f,0.92f,1.0f});
    y += 24.0f;

    auto legend = [&](const glm::vec3& color, const std::string& label)
    {
        appendFilledRect(ui, {x, y + 2.0f, 12.0f, 8.0f}, color);
        appendUiText(ui, x + 20.0f, y, label, 1.30f, {0.90f,0.91f,0.94f});
        y += 17.0f;
    };

    legend({0.88f,0.88f,0.88f}, "ROUTE");
    legend({0.25f,1.0f,0.35f}, "ACTUAL SHIP PATH");
    legend({1.0f,0.25f,0.20f}, "HAZARD PATH");
    legend({0.70f,0.88f,0.72f}, "SHIP BOX");
    legend({0.25f,0.85f,1.0f}, "SHIP NOSE");
    legend({1.0f,0.65f,0.15f}, "TURN / PORTAL POINT");
    legend({1.0f,0.92f,0.15f}, "SELECTED BYPASS TARGET");
    legend({0.20f,0.95f,1.0f}, "REACQUIRE REFERENCE");
    legend({0.85f,0.30f,1.0f}, "ACTIVE PORTAL TARGET");
    legend({1.0f,0.45f,0.05f}, "REPLAN EVENT");

    y += 14.0f;
    appendUiText(ui, x, y, "CONTROLS", 1.55f, {0.92f,0.92f,1.0f});
    y += 23.0f;
    appendUiText(ui, x, y, "RMB ORBIT   MMB PAN", 1.25f, {0.75f,0.78f,0.84f});
    y += 16.0f;
    appendUiText(ui, x, y, "WHEEL ZOOM  SPACE PLAY", 1.25f, {0.75f,0.78f,0.84f});
    y += 16.0f;
    appendUiText(ui, x, y, "[ ] STEP   R NEXT REPLAN", 1.25f, {0.75f,0.78f,0.84f});
    y += 16.0f;
    appendUiText(ui, x, y, "F FIT      ESC CLOSE", 1.25f, {0.75f,0.78f,0.84f});

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

    for (const auto& p : data.routePoints)
        include(p);
    for (const auto& f : data.frames)
    {
        include(f.shipPosition);
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

        if (playButtonRect().contains(x, y))
            state->pendingUiAction = UiAction::TogglePlay;
        else if (prevButtonRect().contains(x, y))
            state->pendingUiAction = UiAction::PreviousFrame;
        else if (nextButtonRect().contains(x, y))
            state->pendingUiAction = UiAction::NextFrame;
        else if (replanButtonRect().contains(x, y))
            state->pendingUiAction = UiAction::NextReplan;
        else if (fitButtonRect().contains(x, y))
            state->pendingUiAction = UiAction::Fit;
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
    else if (key == GLFW_KEY_SPACE)
        state->playing = !state->playing;
    else if (key == GLFW_KEY_F)
        state->requestFit = true;
}

void drawScene(
    PrimitiveRenderer& renderer,
    const trace::TraceDocument& data,
    std::size_t frameIndex,
    const glm::mat4& viewProjection
)
{
    if (data.frames.empty())
        return;

    frameIndex = std::min(frameIndex, data.frames.size() - 1);
    const trace::TraceFrame& frame = data.frames[frameIndex];

    renderer.begin(viewProjection);

    renderer.draw(
        GL_LINES,
        polyline(
            data.routePoints,
            glm::vec3(0.88f, 0.88f, 0.88f)
        ),
        2.0f
    );

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

    std::vector<Vertex> ship;
    appendShipBoxAndArrow(
        ship,
        frame,
        data.shipHalfExtentsMeters
    );
    renderer.draw(GL_LINES, ship, 2.0f);
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

    std::ostringstream title;
    title.setf(std::ios::fixed);
    title.precision(2);
    title
        << "Navigation Runtime 3D - "
        << data.law
        << " | frame " << (frameIndex + 1)
        << "/" << data.frames.size()
        << " | t=" << f.timeSeconds << " s"
        << " | " << f.phase;

    if (!f.plannerStatus.empty())
        title << " | " << f.plannerStatus;

    if (f.hazardActive)
        title << " | clearance=" << f.dynamicClearanceMeters << " m";

    if (f.replanEvent)
        title << " | REPLAN";

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
    const trace::TraceDocument& data
)
{
    const UiAction action = state.pendingUiAction;
    state.pendingUiAction = UiAction::None;

    switch (action)
    {
        case UiAction::TogglePlay:
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

std::string defaultTracePath()
{
#ifdef ELITE_SOURCE_ROOT
    return std::string(ELITE_SOURCE_ROOT) +
        "/tools/navigation_runtime/last_trace_newtonian.json";
#else
    return "tools/navigation_runtime/last_trace_newtonian.json";
#endif
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::string path =
            argc >= 2 ? argv[1] : defaultTracePath();
        const trace::TraceDocument data =
            trace::loadTraceJson(path);

        if (data.frames.empty())
            throw std::runtime_error("trace contains no frames");

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

        GLFWwindow* window =
            glfwCreateWindow(
                1280,
                800,
                "Navigation Runtime 3D",
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
        state.lastRealTime = glfwGetTime();
        fitCamera(state, data);

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

            drawScene(
                renderer,
                data,
                state.frameIndex,
                projection * state.camera.view()
            );

            int windowWidth = 1;
            int windowHeight = 1;
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
            drawHud(
                renderer,
                data,
                state,
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
