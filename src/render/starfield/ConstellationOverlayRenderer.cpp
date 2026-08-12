#include "ConstellationOverlayRenderer.h"

#include <cstddef>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderLibrary.h"

namespace
{
    constexpr float kMinimumDirectionLength = 0.000001f;
    constexpr float kConstellationRadiusScale = 0.992f;
    const glm::vec4 kConstellationLineColor(0.34f, 0.66f, 0.90f, 0.34f);
}

int ConstellationOverlayRenderer::StarReference::catalogId(
    SkyCultureCatalog::StarIdentifier identifier
) const
{
    return identifier == SkyCultureCatalog::StarIdentifier::Hipparcos
        ? hipparcosCatalogId
        : brightStarCatalogId;
}

bool ConstellationOverlayRenderer::StarReference::skyDirection(
    const glm::vec3& observerPositionLy,
    glm::vec3& outDirection
) const
{
    const glm::vec3 relative = positionLy - observerPositionLy;
    const float length = glm::length(relative);
    if (length < kMinimumDirectionLength)
        return false;
    outDirection = relative / length;
    return true;
}

ConstellationOverlayRenderer::~ConstellationOverlayRenderer()
{
    shutdown();
}

bool ConstellationOverlayRenderer::initialize()
{
    if (m_initialized)
        return true;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
        reinterpret_cast<void*>(offsetof(LineVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
        reinterpret_cast<void*>(offsetof(LineVertex, color)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    m_initialized = true;
    return true;
}

void ConstellationOverlayRenderer::shutdown()
{
    if (m_vbo != 0) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao != 0) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    m_vertices.clear();
    m_labelAnchors.clear();
    m_definitions.clear();
    m_initialized = false;
}

void ConstellationOverlayRenderer::setCulture(
    const SkyCultureCatalog::Culture& culture
)
{
    m_starIdentifier = culture.starIdentifier;
    m_definitions = culture.constellations;
    m_vertices.clear();
    m_labelAnchors.clear();
    uploadVertices();
}

void ConstellationOverlayRenderer::rebuild(
    const std::vector<StarReference>& stars,
    const glm::vec3& observerPositionLy,
    float skyRadius
)
{
    if (!m_initialized)
        return;

    std::unordered_map<int, const StarReference*> starsById;
    starsById.reserve(stars.size());
    for (const StarReference& star : stars)
    {
        const int id = star.catalogId(m_starIdentifier);
        if (id > 0)
            starsById.emplace(id, &star);
    }

    m_vertices.clear();
    m_labelAnchors.clear();
    std::size_t sourceSegments = 0;
    std::size_t missingSegments = 0;

    auto directionFor = [&](int id, glm::vec3& out) -> bool
    {
        const auto it = starsById.find(id);
        return it != starsById.end() &&
            it->second->skyDirection(observerPositionLy, out);
    };

    for (std::size_t definitionIndex = 0;
         definitionIndex < m_definitions.size(); ++definitionIndex)
    {
        const ConstellationDefinition& definition = m_definitions[definitionIndex];
        glm::vec3 directionSum(0.0f);
        std::unordered_set<int> anchorStars;

        for (const std::vector<int>& polyline : definition.polylines)
        {
            for (const int id : polyline)
            {
                if (!anchorStars.emplace(id).second)
                    continue;
                glm::vec3 direction;
                if (directionFor(id, direction))
                    directionSum += direction;
            }
        }

        if (glm::length(directionSum) >= kMinimumDirectionLength)
        {
            LabelAnchor anchor;
            anchor.definitionIndex = definitionIndex;
            anchor.skyPosition = glm::normalize(directionSum) * skyRadius *
                kConstellationRadiusScale;
            m_labelAnchors.push_back(anchor);
        }

        for (const std::vector<int>& polyline : definition.polylines)
        {
            for (std::size_t i = 1; i < polyline.size(); ++i)
            {
                ++sourceSegments;
                glm::vec3 directionA;
                glm::vec3 directionB;
                if (!directionFor(polyline[i - 1], directionA) ||
                    !directionFor(polyline[i], directionB))
                {
                    ++missingSegments;
                    continue;
                }

                m_vertices.push_back(LineVertex{
                    directionA * skyRadius * kConstellationRadiusScale,
                    kConstellationLineColor});
                m_vertices.push_back(LineVertex{
                    directionB * skyRadius * kConstellationRadiusScale,
                    kConstellationLineColor});
            }
        }
    }

    uploadVertices();
    std::cout << "[Constellations] segments=" << segmentCount() << "/"
              << sourceSegments << " missing=" << missingSegments << std::endl;
}

void ConstellationOverlayRenderer::uploadVertices()
{
    if (m_vbo == 0)
        return;
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(LineVertex)),
        m_vertices.empty() ? nullptr : m_vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ConstellationOverlayRenderer::render(const glm::mat4& mvp) const
{
    if (!m_initialized || m_vertices.empty())
        return;

    const GLuint shader = ShaderLibrary::instance().get("system_map_lines");
    if (shader == 0)
        return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);
    glUseProgram(shader);

    const GLint mvpLocation = glGetUniformLocation(shader, "uMVP");
    if (mvpLocation >= 0)
        glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);
    glUseProgram(0);
    glLineWidth(1.0f);
}
