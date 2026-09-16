#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderUtils.h"

namespace elite::render::core_legacy
{
// Numeric values are retained only as software-state tokens. They are never
// passed to a Core-profile driver as removed fixed-function pnames/modes.
inline constexpr GLenum ModelViewToken = 0x1700;
inline constexpr GLenum ProjectionToken = 0x1701;
inline constexpr GLenum MatrixModeToken = 0x0BA0;
inline constexpr GLenum CurrentColorToken = 0x0B00;
inline constexpr GLenum QuadsToken = 0x0007;

struct Vertex
{
    glm::vec3 position {0.0f};
    glm::vec4 color {1.0f};
    glm::vec2 texCoord {0.0f};
};

struct State
{
    GLenum matrixMode = ModelViewToken;
    glm::mat4 modelView {1.0f};
    glm::mat4 projection {1.0f};
    std::vector<glm::mat4> modelViewStack;
    std::vector<glm::mat4> projectionStack;

    glm::vec4 color {1.0f};
    glm::vec2 texCoord {0.0f};
    bool texture2D = false;

    bool building = false;
    GLenum primitive = GL_TRIANGLES;
    std::vector<Vertex> vertices;

    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint mvpLocation = -1;
    GLint textureEnabledLocation = -1;
    GLint textureLocation = -1;
};

inline State state;

inline glm::mat4& activeMatrix()
{
    return state.matrixMode == ProjectionToken
        ? state.projection
        : state.modelView;
}

inline std::vector<glm::mat4>& activeStack()
{
    return state.matrixMode == ProjectionToken
        ? state.projectionStack
        : state.modelViewStack;
}

inline void ensureGpuState()
{
    if (state.program != 0)
        return;

    static constexpr const char* vertexShader = R"GLSL(
#version 430 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
uniform mat4 uMvp;
out vec4 vColor;
out vec2 vTexCoord;
void main()
{
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
)GLSL";

    static constexpr const char* fragmentShader = R"GLSL(
#version 430 core
in vec4 vColor;
in vec2 vTexCoord;
uniform bool uTextureEnabled;
uniform sampler2D uTexture;
layout(location = 0) out vec4 fragColor;
void main()
{
    fragColor = uTextureEnabled
        ? texture(uTexture, vTexCoord) * vColor
        : vColor;
}
)GLSL";

    state.program = compileShader(vertexShader, fragmentShader);
    if (state.program == 0)
        return;

    glGenVertexArrays(1, &state.vao);
    glGenBuffers(1, &state.vbo);

    glBindVertexArray(state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(Vertex)),
        reinterpret_cast<const void*>(offsetof(Vertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(Vertex)),
        reinterpret_cast<const void*>(offsetof(Vertex, color))
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(Vertex)),
        reinterpret_cast<const void*>(offsetof(Vertex, texCoord))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    state.mvpLocation = glGetUniformLocation(state.program, "uMvp");
    state.textureEnabledLocation = glGetUniformLocation(state.program, "uTextureEnabled");
    state.textureLocation = glGetUniformLocation(state.program, "uTexture");
}

inline void matrixMode(GLenum mode)
{
    if (mode == ProjectionToken || mode == ModelViewToken)
        state.matrixMode = mode;
}

inline void loadIdentity()
{
    activeMatrix() = glm::mat4(1.0f);
}

inline void loadMatrixf(const GLfloat* matrix)
{
    if (!matrix)
        return;
    activeMatrix() = glm::make_mat4(matrix);
}

inline void pushMatrix()
{
    activeStack().push_back(activeMatrix());
}

inline void popMatrix()
{
    auto& stack = activeStack();
    if (stack.empty())
        return;
    activeMatrix() = stack.back();
    stack.pop_back();
}

inline void ortho(
    GLdouble left,
    GLdouble right,
    GLdouble bottom,
    GLdouble top,
    GLdouble zNear,
    GLdouble zFar
)
{
    activeMatrix() *= glm::ortho(
        static_cast<float>(left),
        static_cast<float>(right),
        static_cast<float>(bottom),
        static_cast<float>(top),
        static_cast<float>(zNear),
        static_cast<float>(zFar)
    );
}

inline void getIntegerv(GLenum pname, GLint* value)
{
    if (!value)
        return;
    if (pname == MatrixModeToken)
    {
        *value = static_cast<GLint>(state.matrixMode);
        return;
    }
    glGetIntegerv(pname, value);
}

inline void getFloatv(GLenum pname, GLfloat* value)
{
    if (!value)
        return;
    if (pname == CurrentColorToken)
    {
        value[0] = state.color.r;
        value[1] = state.color.g;
        value[2] = state.color.b;
        value[3] = state.color.a;
        return;
    }
    glGetFloatv(pname, value);
}

inline void enableTexture2D(bool enabled)
{
    state.texture2D = enabled;
}

inline GLboolean texture2DEnabled()
{
    return state.texture2D ? GL_TRUE : GL_FALSE;
}

inline void color3f(GLfloat r, GLfloat g, GLfloat b)
{
    state.color = glm::vec4(r, g, b, 1.0f);
}

inline void color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    state.color = glm::vec4(r, g, b, a);
}

inline void texCoord2f(GLfloat u, GLfloat v)
{
    state.texCoord = glm::vec2(u, v);
}

inline void texCoord2d(GLdouble u, GLdouble v)
{
    state.texCoord = glm::vec2(static_cast<float>(u), static_cast<float>(v));
}

inline void begin(GLenum primitive)
{
    state.building = true;
    state.primitive = primitive;
    state.vertices.clear();
}

inline void appendVertex(float x, float y, float z)
{
    if (!state.building)
        return;

    Vertex vertex;
    vertex.position = glm::vec3(x, y, z);
    vertex.color = state.color;
    vertex.texCoord = state.texCoord;
    state.vertices.push_back(vertex);
}

inline void vertex2f(GLfloat x, GLfloat y)
{
    appendVertex(x, y, 0.0f);
}

inline void vertex2d(GLdouble x, GLdouble y)
{
    appendVertex(static_cast<float>(x), static_cast<float>(y), 0.0f);
}

inline void vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    appendVertex(x, y, z);
}

inline void drawBufferedVertices()
{
    if (state.vertices.empty())
        return;

    ensureGpuState();
    if (state.program == 0 || state.vao == 0 || state.vbo == 0)
        return;

    GLenum drawMode = state.primitive;
    std::vector<Vertex> converted;
    const Vertex* data = state.vertices.data();
    std::size_t count = state.vertices.size();

    if (state.primitive == QuadsToken)
    {
        converted.reserve((count / 4u) * 6u);
        for (std::size_t i = 0; i + 3u < count; i += 4u)
        {
            converted.push_back(state.vertices[i + 0u]);
            converted.push_back(state.vertices[i + 1u]);
            converted.push_back(state.vertices[i + 2u]);
            converted.push_back(state.vertices[i + 0u]);
            converted.push_back(state.vertices[i + 2u]);
            converted.push_back(state.vertices[i + 3u]);
        }
        drawMode = GL_TRIANGLES;
        data = converted.data();
        count = converted.size();
    }

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    GLint activeTexture = GL_TEXTURE0;

    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);

    glUseProgram(state.program);
    const glm::mat4 mvp = state.projection * state.modelView;
    if (state.mvpLocation >= 0)
        glUniformMatrix4fv(state.mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
    if (state.textureEnabledLocation >= 0)
        glUniform1i(state.textureEnabledLocation, state.texture2D ? 1 : 0);
    if (state.textureLocation >= 0)
        glUniform1i(state.textureLocation, std::max(0, activeTexture - static_cast<GLint>(GL_TEXTURE0)));

    glBindVertexArray(state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(count * sizeof(Vertex)),
        data,
        GL_STREAM_DRAW
    );
    glDrawArrays(drawMode, 0, static_cast<GLsizei>(count));

    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
}

inline void end()
{
    if (!state.building)
        return;
    state.building = false;
    drawBufferedVertices();
    state.vertices.clear();
}
}
