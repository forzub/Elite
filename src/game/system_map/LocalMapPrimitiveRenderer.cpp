#include "src/game/system_map/LocalMapPrimitiveRenderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glad/gl.h>
#include <glm/gtc/constants.hpp>

#include "src/render/ShaderUtils.h"

namespace
{
struct LocalMapPrimitiveGpuState
{
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint viewportSizeLocation = -1;
    GLint colorLocation = -1;
};

LocalMapPrimitiveGpuState& gpuState()
{
    static LocalMapPrimitiveGpuState state;

    if (state.program != 0)
        return state;

    static constexpr const char* vertexShader = R"GLSL(
#version 430 core

layout(location = 0) in vec2 aPositionPx;
uniform vec2 uViewportSize;

void main()
{
    vec2 safeViewport = max(uViewportSize, vec2(1.0));
    vec2 ndc = vec2(
        (aPositionPx.x / safeViewport.x) * 2.0 - 1.0,
        1.0 - (aPositionPx.y / safeViewport.y) * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

    static constexpr const char* fragmentShader = R"GLSL(
#version 430 core

uniform vec4 uColor;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = uColor;
}
)GLSL";

    state.program = compileShader(vertexShader, fragmentShader);
    if (state.program == 0)
        return state;

    glGenVertexArrays(1, &state.vao);
    glGenBuffers(1, &state.vbo);

    glBindVertexArray(state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(glm::vec2)),
        nullptr
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    state.viewportSizeLocation = glGetUniformLocation(state.program, "uViewportSize");
    state.colorLocation = glGetUniformLocation(state.program, "uColor");
    return state;
}

glm::vec4 compatibilityCurrentColor()
{
    GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Transitional bridge for callers not yet migrated to the explicit-color
    // overloads. GL43-B removes these overloads after all callers are moved.
    glGetFloatv(GL_CURRENT_COLOR, color);

    return glm::vec4(color[0], color[1], color[2], color[3]);
}

void drawVertices(
    GLenum primitive,
    const glm::vec2* vertices,
    std::size_t vertexCount,
    const glm::vec4& color
)
{
    if (!vertices || vertexCount == 0)
        return;

    auto& state = gpuState();
    if (state.program == 0 || state.vao == 0 || state.vbo == 0)
        return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    GLint viewport[4] = {0, 0, 1, 1};

    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);

    glUseProgram(state.program);

    if (state.viewportSizeLocation >= 0)
    {
        glUniform2f(
            state.viewportSizeLocation,
            static_cast<float>(std::max(viewport[2], 1)),
            static_cast<float>(std::max(viewport[3], 1))
        );
    }

    if (state.colorLocation >= 0)
        glUniform4fv(state.colorLocation, 1, &color.x);

    glBindVertexArray(state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertexCount * sizeof(glm::vec2)),
        vertices,
        GL_STREAM_DRAW
    );
    glDrawArrays(primitive, 0, static_cast<GLsizei>(vertexCount));

    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
}
}

namespace game::system_map
{
void drawLocalMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::vec4& color
)
{
    const glm::vec2 vertices[] = {
        glm::vec2(static_cast<float>(a.x), static_cast<float>(a.y)),
        glm::vec2(static_cast<float>(b.x), static_cast<float>(b.y))
    };

    drawVertices(GL_LINES, vertices, 2, color);
}

void drawLocalMapCross(
    const glm::dvec2& point,
    float size,
    const glm::vec4& color
)
{
    const glm::vec2 vertices[] = {
        glm::vec2(static_cast<float>(point.x - size), static_cast<float>(point.y)),
        glm::vec2(static_cast<float>(point.x + size), static_cast<float>(point.y)),
        glm::vec2(static_cast<float>(point.x), static_cast<float>(point.y - size)),
        glm::vec2(static_cast<float>(point.x), static_cast<float>(point.y + size))
    };

    drawVertices(GL_LINES, vertices, 4, color);
}

void drawLocalMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments,
    const glm::vec4& color
)
{
    segments = std::max(segments, 8);

    std::vector<glm::vec2> vertices;
    vertices.reserve(static_cast<std::size_t>(segments));

    for (int segment = 0; segment < segments; ++segment)
    {
        const double angle =
            glm::two_pi<double>() *
            static_cast<double>(segment) /
            static_cast<double>(segments);

        vertices.emplace_back(
            static_cast<float>(center.x + std::cos(angle) * radiusPx),
            static_cast<float>(center.y + std::sin(angle) * radiusPx)
        );
    }

    drawVertices(GL_LINE_LOOP, vertices.data(), vertices.size(), color);
}

void drawLocalMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    drawLocalMapLine(a, b, compatibilityCurrentColor());
}

void drawLocalMapCross(
    const glm::dvec2& point,
    float size
)
{
    drawLocalMapCross(point, size, compatibilityCurrentColor());
}

void drawLocalMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
)
{
    drawLocalMapCircle(center, radiusPx, segments, compatibilityCurrentColor());
}
}
