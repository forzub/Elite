#include "src/render/geometry/FarLightsRenderer.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>

#include "src/render/ShaderUtils.h"

namespace render::geometry
{
namespace
{
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 powerArea;
    float kind;
};
} // namespace

FarLightsRenderer::~FarLightsRenderer()
{
    for (auto& [k, s] : m_sets)
    {
        if (s.vbo) glDeleteBuffers(1, &s.vbo);
        if (s.vao) glDeleteVertexArrays(1, &s.vao);
    }
    if (m_program) glDeleteProgram(m_program);
}

bool FarLightsRenderer::enabled()
{
    static const bool on = [] {
        const char* v = std::getenv("ELITE_FAR_LIGHTS");
        return !(v && v[0] == '0');
    }();
    return on;
}

const FarLightsRenderer::Set& FarLightsRenderer::setFor(const void* key, const std::vector<elite::model_asset::AssetLight>& lights)
{
    auto it = m_sets.find(key);
    if (it != m_sets.end())
        return it->second;
    Set s;
    std::vector<Vertex> verts;
    verts.reserve(lights.size());
    for (const auto& l : lights)
        verts.push_back({l.position, l.color, glm::vec2(l.power, l.areaM2), static_cast<float>(l.kind)});
    glGenVertexArrays(1, &s.vao);
    glBindVertexArray(s.vao);
    glGenBuffers(1, &s.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, powerArea)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, kind)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    s.count = static_cast<GLsizei>(verts.size());
    std::cerr << "[FarLights] light set " << key << ": " << s.count << " lights" << std::endl;
    return m_sets.emplace(key, s).first->second;
}

void FarLightsRenderer::queue(const void* key, const std::vector<elite::model_asset::AssetLight>& lights, const glm::mat4& modelView)
{
    if (lights.empty())
        return;
    m_items.push_back({&setFor(key, lights), modelView});
}

void FarLightsRenderer::flush(const glm::mat4& proj, float focalPx, float nearPlane, float timeSeconds)
{
    m_lastPoints = 0;
    if (m_items.empty())
        return;
    if (!m_program)
        m_program = compileShaderFromFiles("assets/shaders/mesh/far_lights.vert", "assets/shaders/mesh/far_lights.frag");
    static const float gain = [] {
        const char* v = std::getenv("ELITE_FAR_LIGHTS_GAIN");
        return v && *v ? static_cast<float>(std::atof(v)) : 1.0f;
    }();
    GLboolean depthMask = GL_TRUE, blend = glIsEnabled(GL_BLEND), depthTest = glIsEnabled(GL_DEPTH_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    GLint depthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(m_program);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniform1f(glGetUniformLocation(m_program, "uFocalPx"), focalPx);
    glUniform1f(glGetUniformLocation(m_program, "uNear"), nearPlane);
    glUniform1f(glGetUniformLocation(m_program, "uTime"), timeSeconds);
    glUniform1f(glGetUniformLocation(m_program, "uGain"), gain);
    const GLint mvLoc = glGetUniformLocation(m_program, "uModelView");
    for (const auto& it : m_items)
    {
        glUniformMatrix4fv(mvLoc, 1, GL_FALSE, &it.modelView[0][0]);
        glBindVertexArray(it.set->vao);
        glDrawArrays(GL_POINTS, 0, it.set->count);
        m_lastPoints += static_cast<std::size_t>(it.set->count);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glDepthMask(depthMask);
    glDepthFunc(static_cast<GLenum>(depthFunc));
    if (!blend) glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!depthTest) glDisable(GL_DEPTH_TEST);
    glDisable(GL_PROGRAM_POINT_SIZE);
    m_items.clear();
}
} // namespace render::geometry
