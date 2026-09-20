#include "NavigationTrace.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
uniform mat4 uViewProjection;
out vec3 vertexColor;
void main()
{
    vertexColor = inColor;
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
)";

    static constexpr const char* FragmentShader = R"(
#version 330 core
in vec3 vertexColor;
out vec4 outColor;
void main()
{
    outColor = vec4(vertexColor, 1.0);
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
        std::min(lastInclusive + 1, points.size());
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

void appendShipBoxAndArrow(
    std::vector<Vertex>& out,
    const trace::TraceFrame& frame,
    const glm::dvec3& halfExtents
)
{
    const glm::vec3 center = toVec3(frame.shipPosition);
    glm::vec3 forward = toVec3(frame.shipForward);
    if (glm::length(forward) <= 1.0e-6f)
        forward = {1.0f, 0.0f, 0.0f};
    forward = glm::normalize(forward);

    glm::vec3 upSeed(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(forward, upSeed)) > 0.92f)
        upSeed = {1.0f, 0.0f, 0.0f};

    const glm::vec3 right =
        glm::normalize(glm::cross(forward, upSeed));
    const glm::vec3 up =
        glm::normalize(glm::cross(right, forward));

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
