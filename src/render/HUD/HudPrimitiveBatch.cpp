#include "src/render/HUD/HudPrimitiveBatch.h"

#include <cmath>

#include "src/render/ShaderUtils.h"

namespace render::hud
{

HudPrimitiveBatch::~HudPrimitiveBatch()
{
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_program) glDeleteProgram(m_program);
}

void HudPrimitiveBatch::init()
{
    if (m_program)
        return;

    m_program = compileShaderFromFiles(
        "assets/shaders/hud/hudPrimitive.vert",
        "assets/shaders/common/vertex_color.frag"
    );

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 4096, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0)
    );
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(sizeof(glm::vec2))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void HudPrimitiveBatch::begin(int screenWidth, int screenHeight)
{
    if (!m_program)
        init();
    m_width = screenWidth > 0 ? screenWidth : 1;
    m_height = screenHeight > 0 ? screenHeight : 1;
    m_vertices.clear();
}

glm::vec2 HudPrimitiveBatch::pxToNdc(const glm::vec2& value) const
{
    return {
        2.0f * value.x / static_cast<float>(m_width) - 1.0f,
        1.0f - 2.0f * value.y / static_cast<float>(m_height)
    };
}

void HudPrimitiveBatch::triangle(
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec2& c,
    const glm::vec4& color
)
{
    m_vertices.push_back({pxToNdc(a), color});
    m_vertices.push_back({pxToNdc(b), color});
    m_vertices.push_back({pxToNdc(c), color});
}

void HudPrimitiveBatch::line(
    const glm::vec2& aPx,
    const glm::vec2& bPx,
    float thicknessPx,
    const glm::vec4& color
)
{
    const glm::vec2 delta = bPx - aPx;
    const float length = glm::length(delta);
    if (length <= 1.0e-4f)
        return;

    const glm::vec2 normal =
        glm::vec2(-delta.y, delta.x) / length * (thicknessPx * 0.5f);
    const glm::vec2 p0 = aPx + normal;
    const glm::vec2 p1 = bPx + normal;
    const glm::vec2 p2 = bPx - normal;
    const glm::vec2 p3 = aPx - normal;
    triangle(p0, p1, p2, color);
    triangle(p0, p2, p3, color);
}

void HudPrimitiveBatch::flush()
{
    if (!m_program || m_vertices.empty())
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_program);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
        m_vertices.data(),
        GL_DYNAMIC_DRAW
    );
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    m_vertices.clear();
}

} // namespace render::hud
