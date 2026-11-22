#include "src/render/cockpit/DepthLineBatch.h"

#include <cmath>

#include "src/render/ShaderUtils.h"

namespace render::cockpit
{
DepthLineBatch::~DepthLineBatch()
{
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_program) glDeleteProgram(m_program);
}

void DepthLineBatch::init()
{
    if (m_program)
        return;
    m_program = compileShaderFromFiles("assets/shaders/hud/hudPrimitiveDepth.vert", "assets/shaders/common/vertex_color.frag");
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 4096, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(glm::vec4)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void DepthLineBatch::begin(int width, int height)
{
    if (!m_program)
        init();
    m_width = width > 0 ? width : 1;
    m_height = height > 0 ? height : 1;
    m_vertices.clear();
}

void DepthLineBatch::line(const glm::vec4& a, const glm::vec4& b, float thicknessPx, const glm::vec4& color)
{
    if (!(a.w > 1.0e-6f) || !(b.w > 1.0e-6f))
        return;
    const glm::vec2 na = glm::vec2(a) / a.w, nb = glm::vec2(b) / b.w;
    // the direction in pixels -> the normal offset in NDC
    const glm::vec2 px((nb.x - na.x) * 0.5f * m_width, (nb.y - na.y) * 0.5f * m_height);
    const float len = glm::length(px);
    if (len < 1.0e-4f || !std::isfinite(len))
        return;
    const glm::vec2 nPx = glm::vec2(-px.y, px.x) / len * (0.5f * thicknessPx);
    const glm::vec2 nNdc(nPx.x * 2.0f / m_width, nPx.y * 2.0f / m_height);
    const glm::vec4 a0 = a + glm::vec4(nNdc * a.w, 0.0f, 0.0f), a1 = a - glm::vec4(nNdc * a.w, 0.0f, 0.0f);
    const glm::vec4 b0 = b + glm::vec4(nNdc * b.w, 0.0f, 0.0f), b1 = b - glm::vec4(nNdc * b.w, 0.0f, 0.0f);
    m_vertices.push_back({a0, color});
    m_vertices.push_back({b0, color});
    m_vertices.push_back({b1, color});
    m_vertices.push_back({a0, color});
    m_vertices.push_back({b1, color});
    m_vertices.push_back({a1, color});
}

void DepthLineBatch::flush()
{
    if (!m_program || m_vertices.empty())
        return;
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLint funcWas = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &funcWas);
    GLboolean maskWas = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &maskWas);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_program);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    glDepthMask(maskWas);
    glDepthFunc(static_cast<GLenum>(funcWas));
    if (!depthWas)
        glDisable(GL_DEPTH_TEST);
    m_vertices.clear();
}
} // namespace render::cockpit
